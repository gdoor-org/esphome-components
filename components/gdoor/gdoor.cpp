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
#include "gdoor.h"
#include "esphome/core/log.h"
#include "driver/dac_oneshot.h"

using esphome::esp_log_printf_;

static const char *TAG = "gdoor_esphome.gdoor";

namespace GDOOR {
    /*
    * Setup everything needed for GDoor.
    * @param int txpin Pin number where PWM is created when sending out data
    * @param int txenpin Pin number where output buffer is turned on/off
    * @param int rxpin Pin number where pulses from bus are received
    */
    void setup(uint8_t txpin, uint8_t txenpin, uint8_t rxpin) {
        GDOOR_RX::setup(rxpin);
        GDOOR_TX::setup(txpin, txenpin);
    }

    /*
    * RX loop, needs to be called in main loop()
    * Needed for the decoding logic.
    */
    void loop() {
        GDOOR_RX::loop();
        GDOOR_TX::loop();
    }

    /**
    * User function, called to see if new data is available.
    * @return Data pointer as GDOOR_RX_DATA class or NULL if no data is available
    */
    GDOOR_DATA* read() {
        return GDOOR_RX::read();
    }

    /*
    * Send out data.
    * @param data buffer with bus data
    * @param len length of buffer, can be max MAX_WORDLEN
    */
    void send(uint8_t *data, uint16_t len) {
        GDOOR_TX::send(data, len);
    }

    /*
    * Send out data.
    * @param hex string data without 0x prefix
    */
    void send(const char *str) {
        GDOOR_TX::send(str);
    }

    /*
    * GDOOR activity status
    * @return true: GDOOR RX or TX is active. False: no GDOOR activity.
    */
    bool active() {
        return (GDOOR_TX::busy() || GDOOR_RX::rx_state != 0);
    }

    /** Set RX Threshold (Sensitivity) to a certain level,
     * only working for IO22 rx input on v3.1 hardware
    */
   void setRxThreshold(uint8_t pin, float sensitivity) {
        uint8_t value = (uint8_t)((sensitivity / 3.3f) * 255);
        // GPIO25 = DAC_CHAN_0, GPIO26 = DAC_CHAN_1 (IDF v5 dac_oneshot API)
        dac_channel_t chan = (pin == 25) ? DAC_CHAN_0 : DAC_CHAN_1;
        dac_oneshot_handle_t handle;
        dac_oneshot_config_t cfg = { .chan_id = chan };
        dac_oneshot_new_channel(&cfg, &handle);
        dac_oneshot_output_voltage(handle, value);
        // handle intentionally not deleted — DAC output must remain active
   }
}