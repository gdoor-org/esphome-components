/*
 * This file is part of the GDoor distribution (https://github.com/gdoor-org).
 * Copyright (c) 2024 GDoor authors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * TX implementation for ESPHome >= v2025.6.3 (ESP-IDF / Arduino-ESP32 v3).
 *
 * Strategy: mirrors gdoor-alt exactly, only the hardware API wrappers change:
 *   - hw_timer_t  → ESP-IDF GPTIMER (driver/gptimer.h)
 *   - ledcWrite(channel, duty) → ledc_set_duty / ledc_update_duty (IDF, ISR-safe)
 *   - timerStart/timerStop from ISR → "always-running timer + tx_active flag" pattern
 *   - GDOOR_RX::enable() deferred from ISR to loop() (attachInterrupt not ISR-safe)
 */

#include "defines.h"
#include "gdoor_tx.h"
#include "gdoor_rx.h"
#include "gdoor_utils.h"
#include "esphome/core/log.h"

static const char *TAG = "gdoor_esphome.gdoor_tx";

namespace GDOOR_TX {

    // -------------------------------------------------------------------------
    // State (mirrors gdoor-alt)
    // -------------------------------------------------------------------------
    static volatile uint16_t tx_state    = 0;
    static volatile uint16_t tx_words[MAX_WORDLEN];
    static volatile uint16_t bits_len    = 0;
    static volatile uint16_t bits_ptr    = 0;
    static volatile uint16_t pulse_cnt   = 0;
    static volatile uint8_t  startbit_send = 0;
    static volatile uint8_t  timer_oc_state = 0;

    // GPTIMER design: timer runs always; ISR is gated by tx_active flag.
    // tx_just_done signals loop() to call GDOOR_RX::enable() in main context.
    static volatile bool tx_active    = false;
    static volatile bool tx_just_done = false;

    static gptimer_handle_t timer_60khz = nullptr;
    static ledc_channel_t   ledc_ch     = LEDC_CHANNEL_0; // cached at setup

    static uint8_t pin_tx    = 0;
    static uint8_t pin_tx_en = 0;

    // Hex digit lookup — replaces Arduino String hexChars
    static inline int hex_digit(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // -------------------------------------------------------------------------
    // Helpers (identical to gdoor-alt)
    // -------------------------------------------------------------------------
    static inline uint16_t byte2word(uint8_t byte) {
        uint16_t value = byte & 0x00FF;
        if (GDOOR_UTILS::parity_odd(byte)) {
            value |= 0x100;
        }
        return value;
    }

    // -------------------------------------------------------------------------
    // start_timer — called from main context only
    // -------------------------------------------------------------------------
    static inline void start_timer() {
        tx_state |= STATE_SENDING;
        bits_ptr      = 0;
        pulse_cnt     = 0;
        timer_oc_state = 0;
        startbit_send  = 0;

        GDOOR_RX::disable();                              // 1. detach RX interrupt FIRST
        gpio_set_level((gpio_num_t)pin_tx_en, 1);         // 2. enable bus driver
        tx_active = true;                                  // 3. open ISR gate
    }

    // -------------------------------------------------------------------------
    // stop_timer_from_isr — called from ISR context only
    // All operations must be ISR-safe (register writes only, no RTOS calls).
    // -------------------------------------------------------------------------
    static inline void IRAM_ATTR stop_timer_from_isr() {
        // Carrier OFF — pure IDF register writes, ISR-safe
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ledc_ch, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ledc_ch);

        // TX_EN LOW — IDF gpio_set_level is ISR-safe (hw register write)
        gpio_set_level((gpio_num_t)pin_tx_en, 0);

        // Update state
        tx_state  &= (uint16_t)~STATE_SENDING;
        tx_active  = false;
        tx_just_done = true;   // signal loop() to re-enable RX
        // NOTE: GDOOR_RX::enable() is intentionally NOT called here;
        // attachInterrupt() is not ISR-safe and is deferred to loop().
    }

    // -------------------------------------------------------------------------
    // ISR — fires every 16.67 µs (60 kHz), logic is 1:1 from gdoor-alt
    // -------------------------------------------------------------------------
    static bool IRAM_ATTR isr_timer_60khz(
        gptimer_handle_t /*timer*/,
        const gptimer_alarm_event_data_t * /*edata*/,
        void * /*user_ctx*/)
    {
        if (!tx_active) return false; // gate: instant exit when idle

        if (pulse_cnt == 0) {
            // Current phase (burst or pause) is finished — decide what comes next.

            if (bits_ptr >= bits_len || bits_ptr >= (uint16_t)(MAX_WORDLEN * 9)) {
                // All bits sent — stop.
                stop_timer_from_isr();
                return false;
            }

            if (timer_oc_state == 1) {
                // Just finished a carrier burst → now send inter-bit pause (silence).
                timer_oc_state = 0;
                pulse_cnt = PAUSE_PULSENUM;
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ledc_ch, 0);  // carrier OFF
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ledc_ch);
            } else {
                // Just finished a pause → now send next carrier burst.
                if (!startbit_send) {
                    // First burst is the start bit (fixed length, not in tx_words).
                    pulse_cnt     = STARTBIT_PULSENUM;
                    startbit_send = 1;
                } else {
                    // Load the next data bit (LSB-first, 9 bits per word).
                    uint8_t wordindex = (uint8_t)(bits_ptr / 9);
                    uint8_t bitindex  = (uint8_t)(bits_ptr % 9);
                    pulse_cnt = (tx_words[wordindex] & (uint16_t)(1u << bitindex))
                                    ? ONE_PULSENUM : ZERO_PULSENUM;
                    bits_ptr++;
                }
                timer_oc_state = 1;                              // next phase: pause
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ledc_ch, 127); // carrier ON (50% duty)
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ledc_ch);
            }
        } else {
            pulse_cnt--;
        }

        return false; // no high-priority task woken
    }

    // -------------------------------------------------------------------------
    // setup — called once from GdoorComponent::setup()
    // -------------------------------------------------------------------------
    void setup(uint8_t txpin, uint8_t txenpin) {
        pin_tx    = txpin;
        pin_tx_en = txenpin;

        // --- GPIO outputs ---
        gpio_set_direction((gpio_num_t)pin_tx_en, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)pin_tx_en, 0);
        // pin_tx direction is set by LEDC channel config below

        // --- LEDC carrier: 52 kHz, 8-bit resolution (same frequency as gdoor-alt) ---
        // Timer config — use LEDC_TIMER_1 (LEDC_TIMER_0 reserved for other use)
        ledc_timer_config_t ledc_timer_cfg = {};
        ledc_timer_cfg.speed_mode      = LEDC_LOW_SPEED_MODE;
        ledc_timer_cfg.timer_num       = LEDC_TIMER_1;
        ledc_timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
        ledc_timer_cfg.freq_hz         = 52000;
        ledc_timer_cfg.clk_cfg         = LEDC_AUTO_CLK;
        ledc_timer_config(&ledc_timer_cfg);

        // Channel config — use LEDC_CHANNEL_0, duty=0 (carrier off initially)
        ledc_ch = LEDC_CHANNEL_0;
        ledc_channel_config_t ledc_ch_cfg = {};
        ledc_ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_ch_cfg.channel    = ledc_ch;
        ledc_ch_cfg.timer_sel  = LEDC_TIMER_1;
        ledc_ch_cfg.intr_type  = LEDC_INTR_DISABLE;
        ledc_ch_cfg.gpio_num   = (int)pin_tx;
        ledc_ch_cfg.duty       = 0;
        ledc_ch_cfg.hpoint     = 0;
        ledc_channel_config(&ledc_ch_cfg);

        // --- GPTIMER: 60 kHz resolution → fires ISR every 16.67 µs ---
        gptimer_config_t timer_config = {};
        timer_config.clk_src     = GPTIMER_CLK_SRC_DEFAULT;
        timer_config.direction   = GPTIMER_COUNT_UP;
        timer_config.resolution_hz = 60000; // 60 kHz
        gptimer_new_timer(&timer_config, &timer_60khz);

        gptimer_event_callbacks_t cbs = {};
        cbs.on_alarm = isr_timer_60khz;
        gptimer_register_event_callbacks(timer_60khz, &cbs, nullptr);

        gptimer_alarm_config_t alarm_config = {};
        alarm_config.alarm_count  = 1;   // alarm every 1 tick = every 16.67 µs
        alarm_config.reload_count = 0;
        alarm_config.flags.auto_reload_on_alarm = true;
        gptimer_set_alarm_action(timer_60khz, &alarm_config);

        // Enable and start — timer runs always; ISR returns immediately when
        // tx_active == false, keeping idle overhead negligible (~3 µs/ms).
        gptimer_enable(timer_60khz);
        gptimer_start(timer_60khz);

        // Initial state
        tx_active    = false;
        tx_just_done = false;
        tx_state     = 0;
        bits_len     = 0;

        ESP_LOGCONFIG(TAG, "GDoor TX setup:");
        ESP_LOGCONFIG(TAG, "  TX pin      : GPIO %u", pin_tx);
        ESP_LOGCONFIG(TAG, "  TX EN pin   : GPIO %u", pin_tx_en);
        ESP_LOGCONFIG(TAG, "  Carrier     : 52000 Hz");
        ESP_LOGCONFIG(TAG, "  Timer       : 60000 Hz (GPTIMER)");
        ESP_LOGCONFIG(TAG, "  LEDC ch     : %u", (uint8_t)ledc_ch);
    }

    // -------------------------------------------------------------------------
    // send (byte buffer) — called from main context
    // -------------------------------------------------------------------------
    void send(uint8_t *data, uint16_t len) {
        if ((tx_state & STATE_SENDING) || len >= MAX_WORDLEN) return;

        bits_ptr  = 0;
        pulse_cnt = 0;

        // Build 9-bit words (8 data + 1 odd-parity), LSB-first
        for (uint16_t i = 0; i < len; i++) {
            tx_words[i] = byte2word(data[i]);
        }
        // Append CRC (sum of all data bytes) as the final word
        uint8_t crc = GDOOR_UTILS::crc(data, len);
        tx_words[len] = byte2word(crc);

        // bits_len = data words + CRC word, each 9 bits.
        // The start bit is NOT counted here; the ISR handles it separately
        // via startbit_send, matching the original gdoor-alt design.
        bits_len = (uint16_t)((len + 1) * 9);

        ESP_LOGV(TAG, "TX send: %u bytes + CRC 0x%02X, bits_len=%u", len, (unsigned)crc, bits_len);
        start_timer();
    }

    // -------------------------------------------------------------------------
    // send (hex string) — accepts a C string of hex pairs (e.g. "A1B2C3")
    // -------------------------------------------------------------------------
    static uint8_t tx_strbuffer[MAX_WORDLEN * 2]; // module-level parse buffer

    void send(const char *str) {
        if (!str || *str == '\0') return;
        size_t slen = strlen(str);
        if (slen >= (size_t)(MAX_WORDLEN * 2)) return;

        uint16_t index = 0;
        for (size_t i = 0; i + 1 < slen; i += 2) {
            int high = hex_digit(str[i]);
            int low  = hex_digit(str[i + 1]);
            if (high < 0 || low < 0) {
                index = 0;
                break;
            }
            tx_strbuffer[index++] = (uint8_t)((high << 4) | low);
        }
        if (index > 0) {
            send(tx_strbuffer, index);
        }
    }

    // -------------------------------------------------------------------------
    // loop — must be called from GdoorComponent::loop() / GDOOR::loop()
    // Deferred RX re-enable after TX completes (attachInterrupt not ISR-safe).
    // -------------------------------------------------------------------------
    void loop() {
        if (tx_just_done) {
            tx_just_done = false;
            // enable() clears state + disables pending timer alarms + re-attaches interrupt.
            // Discards any stale RX data that was captured from our own TX signal.
            GDOOR_RX::enable();
            ESP_LOGV(TAG, "TX done, RX re-enabled");
        }
    }

    // -------------------------------------------------------------------------
    // busy — replaces tx_state extern used in gdoor-alt's active() check
    // -------------------------------------------------------------------------
    bool busy() {
        return (tx_state & STATE_SENDING) != 0;
    }

} // namespace GDOOR_TX
