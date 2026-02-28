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
 * RX implementation for ESPHome >= v2025.6.3 (ESP-IDF / Arduino-ESP32 v3).
 *
 * Strategy: mirrors gdoor-alt exactly, only hardware API wrappers change:
 *   - hw_timer_t  → ESP-IDF GPTIMER (driver/gptimer.h)
 *   - timerWrite(timer, 0) / timerStart() from GPIO ISR
 *       → gptimer_get_raw_count() + gptimer_set_alarm_action() from GPIO ISR
 *         (both are ISR-safe via portENTER_CRITICAL spinlocks)
 *   - timerStop() from timer callbacks
 *       → alarm auto-disables after firing (auto_reload_on_alarm = false)
 *   - "always running timer + alarm deadline" replaces start/stop per edge
 *
 * Timing (120 kHz = 8.33 µs/tick):
 *   BIT_TIMEOUT_TICKS       = 20  → 166.7 µs  (bit-end detection)
 *   BITSTREAM_TIMEOUT_TICKS = 270 → 2250  µs  (frame-end detection)
 */

#include "defines.h"
#include "gdoor_rx.h"
#include "gdoor_data.h"
#include "gdoor_utils.h"
#include "esphome/core/log.h"

static const char *TAG = "gdoor_esphome.gdoor_rx";

// Fires 166.7 µs after the last carrier edge → burst ended, store count.
// Matches old: timerAlarmWrite(timer_bit_received, 20, true) at 120kHz.
#define BIT_TIMEOUT_TICKS       20u

// Fires 2250 µs after the last carrier edge → entire frame ended.
// Matches old: timerAlarmWrite(timer_bitstream_received, 6*STARTBIT_MIN_LEN, true)
// = 6 × 45 = 270 ticks at 120kHz.
#define BITSTREAM_TIMEOUT_TICKS (6u * STARTBIT_MIN_LEN)  // = 270

namespace GDOOR_RX {

    // -------------------------------------------------------------------------
    // State — all accessed from ISR context, so volatile
    // -------------------------------------------------------------------------
    static volatile uint16_t counts[MAX_WORDLEN * 9]; // pulse counts per bit burst
    static volatile uint16_t isr_cnt    = 0;           // edges counted in current burst
    static volatile uint8_t  bitcounter = 0;           // number of complete bits stored

    uint16_t rx_state = 0; // state flags (extern in header for active() check)

    static GDOOR_DATA retval;

    static gptimer_handle_t timer_bit_received       = nullptr;
    static gptimer_handle_t timer_bitstream_received = nullptr;

    static uint8_t pin_rx = 0;

    // -------------------------------------------------------------------------
    // reset_state — clears counters and disables both timer alarms.
    // Does NOT touch rx_state so FLAG_DATA_READY survives until read().
    // Called from enable(), disable(), and loop() after parse.
    // -------------------------------------------------------------------------
    static void reset_state() {
        bitcounter = 0;
        isr_cnt    = 0;
        // Passing nullptr disables the alarm (no new firing until GPIO ISR re-arms).
        if (timer_bit_received)
            gptimer_set_alarm_action(timer_bit_received, nullptr);
        if (timer_bitstream_received)
            gptimer_set_alarm_action(timer_bitstream_received, nullptr);
    }

    // -------------------------------------------------------------------------
    // GPIO ISR — fires on every FALLING edge of the 60 kHz carrier burst.
    //
    // For each edge:
    //   1. Mark RX as active
    //   2. Count the edge
    //   3. Re-arm the bit-end   alarm: deadline = now + BIT_TIMEOUT_TICKS
    //   4. Re-arm the frame-end alarm: deadline = now + BITSTREAM_TIMEOUT_TICKS
    //
    // Both gptimer_get_raw_count() and gptimer_set_alarm_action() are ISR-safe
    // (they use portENTER_CRITICAL spinlocks internally — pure register ops).
    // -------------------------------------------------------------------------
    static void IRAM_ATTR isr_extint_rx(void * /*arg*/) {
        rx_state |= (uint16_t)FLAG_RX_ACTIVE;
        isr_cnt++;

        uint64_t now;
        gptimer_alarm_config_t alarm = {};
        alarm.flags.auto_reload_on_alarm = false; // one-shot: auto-disables after firing

        // Bit-end alarm
        (void)gptimer_get_raw_count(timer_bit_received, &now);
        alarm.alarm_count = now + BIT_TIMEOUT_TICKS;
        (void)gptimer_set_alarm_action(timer_bit_received, &alarm);

        // Frame-end alarm
        (void)gptimer_get_raw_count(timer_bitstream_received, &now);
        alarm.alarm_count = now + BITSTREAM_TIMEOUT_TICKS;
        (void)gptimer_set_alarm_action(timer_bitstream_received, &alarm);
    }

    // -------------------------------------------------------------------------
    // GPTIMER callback: bit burst ended (no new edge for BIT_TIMEOUT_TICKS).
    // Stores the edge count for the completed burst; resets the edge counter.
    // Mirrors old isr_timer_bit_received() exactly.
    // -------------------------------------------------------------------------
    static bool IRAM_ATTR cb_bit_received(
        gptimer_handle_t /*timer*/,
        const gptimer_alarm_event_data_t * /*edata*/,
        void * /*user_ctx*/)
    {
        if (bitcounter >= (uint8_t)(MAX_WORDLEN * 9)) {
            bitcounter = 0; // guard against buffer overrun
        }
        counts[bitcounter] = isr_cnt;
        isr_cnt = 0;
        bitcounter++;
        // Alarm auto-disables (auto_reload_on_alarm = false).
        // It will be re-armed by the next GPIO edge.
        return false; // no high-priority task woken
    }

    // -------------------------------------------------------------------------
    // GPTIMER callback: frame ended (no new edge for BITSTREAM_TIMEOUT_TICKS).
    // Signals loop() that a complete frame is ready for parsing.
    // Mirrors old isr_timer_bitstream_received() exactly.
    // -------------------------------------------------------------------------
    static bool IRAM_ATTR cb_bitstream_received(
        gptimer_handle_t /*timer*/,
        const gptimer_alarm_event_data_t * /*edata*/,
        void * /*user_ctx*/)
    {
        rx_state &= (uint16_t)~FLAG_RX_ACTIVE;
        rx_state |= (uint16_t)FLAG_BITSTREAM_RECEIVED;
        // Alarm auto-disables after firing.
        return false;
    }

    // -------------------------------------------------------------------------
    // enable / disable — RX interrupt gate, called by GDOOR_TX around TX bursts.
    // Both mirror gdoor-alt: reset state on both enter and exit.
    // -------------------------------------------------------------------------
    void enable() {
        rx_state = 0;      // clear all flags including any stale state
        reset_state();     // clear counters, disable pending timer alarms
        gpio_isr_handler_add((gpio_num_t)pin_rx, isr_extint_rx, nullptr);
    }

    void disable() {
        gpio_isr_handler_remove((gpio_num_t)pin_rx);  // stop new edges first
        rx_state = 0;
        reset_state();
    }

    // -------------------------------------------------------------------------
    // setup — called once from GdoorComponent::setup()
    // -------------------------------------------------------------------------
    void setup(uint8_t rxpin) {
        pin_rx = rxpin;
        // Configure as plain input — active comparator output; no pullup (INPUT_PULLUP
        // would load the comparator at 45kΩ and distort the threshold).
        gpio_config_t io_conf = {};
        io_conf.intr_type    = GPIO_INTR_NEGEDGE;  // FALLING edge trigger
        io_conf.mode         = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << pin_rx);
        io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_config(&io_conf);

        // Install per-GPIO ISR service; ESP_ERR_INVALID_STATE means already installed.
        esp_err_t err = gpio_install_isr_service(0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "gpio_install_isr_service failed: %d", err);
        }

        retval.len   = 0;
        retval.valid = 0;

        // Shared GPTIMER config: 120 kHz resolution, count up
        gptimer_config_t timer_config = {};
        timer_config.clk_src       = GPTIMER_CLK_SRC_DEFAULT;
        timer_config.direction     = GPTIMER_COUNT_UP;
        timer_config.resolution_hz = TIMER_FREQ_RX; // 120000

        // --- Bit-end timer ---
        gptimer_new_timer(&timer_config, &timer_bit_received);

        gptimer_event_callbacks_t cbs = {};
        cbs.on_alarm = cb_bit_received;
        gptimer_register_event_callbacks(timer_bit_received, &cbs, nullptr);

        // Alarm disabled initially (nullptr); GPIO ISR will arm it on first edge.
        gptimer_set_alarm_action(timer_bit_received, nullptr);
        gptimer_enable(timer_bit_received);
        gptimer_start(timer_bit_received); // always running; alarm deadline set per-edge

        // --- Frame-end timer ---
        gptimer_new_timer(&timer_config, &timer_bitstream_received);

        cbs.on_alarm = cb_bitstream_received;
        gptimer_register_event_callbacks(timer_bitstream_received, &cbs, nullptr);

        gptimer_set_alarm_action(timer_bitstream_received, nullptr);
        gptimer_enable(timer_bitstream_received);
        gptimer_start(timer_bitstream_received); // always running

        ESP_LOGCONFIG(TAG, "GDoor RX setup:");
        ESP_LOGCONFIG(TAG, "  RX pin            : GPIO %u", pin_rx);
        ESP_LOGCONFIG(TAG, "  Timer resolution  : %u Hz", TIMER_FREQ_RX);
        ESP_LOGCONFIG(TAG, "  Bit timeout       : %u ticks (%.0f µs)",
                      BIT_TIMEOUT_TICKS,
                      BIT_TIMEOUT_TICKS * 1e6f / TIMER_FREQ_RX);
        ESP_LOGCONFIG(TAG, "  Bitstream timeout : %u ticks (%.0f µs)",
                      BITSTREAM_TIMEOUT_TICKS,
                      BITSTREAM_TIMEOUT_TICKS * 1e6f / TIMER_FREQ_RX);

        // Enable external interrupt last
        enable();
    }

    // -------------------------------------------------------------------------
    // loop — called from GdoorComponent::loop() via GDOOR::loop().
    // Detects frame completion, parses, then resets counters for next frame.
    // -------------------------------------------------------------------------
    void loop() {
        if (rx_state & FLAG_BITSTREAM_RECEIVED) {
            rx_state &= (uint16_t)~FLAG_BITSTREAM_RECEIVED;
            ESP_LOGVV(TAG, "Gira RX done, bits=%u", bitcounter);
            if (retval.parse(const_cast<uint16_t *>(counts), bitcounter)) {
                ESP_LOGVV(TAG, "Gira RX parsed OK");
                rx_state |= FLAG_DATA_READY; // preserved through reset_state()
            }
            reset_state(); // clear counters + disable alarms; FLAG_DATA_READY survives
        }
    }

    // -------------------------------------------------------------------------
    // read — return parsed frame data if available
    // -------------------------------------------------------------------------
    GDOOR_DATA* read() {
        if (rx_state & FLAG_DATA_READY) {
            rx_state &= (uint16_t)~FLAG_DATA_READY;
            return &retval;
        }
        return nullptr;
    }

} // namespace GDOOR_RX
