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
#ifndef GDOOR_TX_H

#define GDOOR_TX_H
#include "gdoor_print.h"
#include "driver/gptimer.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

namespace GDOOR_TX { //Namespace as we can only use it once
    void loop();    // checks for TX completion, re-enables RX in main context
    void send(uint8_t *words, uint16_t len);
    void send(const char *str);
    void setup(uint8_t txpin, uint8_t txenpin);
    bool busy();
};

#endif
