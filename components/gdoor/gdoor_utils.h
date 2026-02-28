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
#ifndef GDOOR_UTILS_H
#define GDOOR_UTILS_H
#include "gdoor_print.h"

namespace GDOOR_UTILS {
    uint8_t crc(uint8_t *words, uint16_t len);
    uint8_t parity_odd(uint8_t word);

    // Print an integer as uppercase hex without leading zeros.
    static inline size_t _print_hex_upper(Print& p, uint32_t v) {
        static const char HC[] = "0123456789ABCDEF";
        char buf[9]; buf[8] = '\0'; int i = 8;
        do { buf[--i] = HC[v & 0xF]; v >>= 4; } while (v);
        return p.print(&buf[i]);
    }

    uint16_t divider(uint32_t frequency);

    /*
    * Template Function (needs to live in header file),
    * used to print out json hex array.
    * 
    * "keyname": {"0xdata[0]", ..., "0xdata[len-1]"}
    */
    template<typename T> size_t print_json_hexarray(Print& p, const char *keyname, const T* data, const uint16_t len) {
        size_t r = 0;
        r+= p.print("\"");
        r+= p.print(keyname);
        r+= p.print("\": [");
        for(uint16_t i=0; i<len; i++) {
            r+= p.print("\"0x");
            r+= _print_hex_upper(p, (uint32_t)data[i]);
            if (i==len-1) {
                r+= p.print("\"");
            } else {
                r+= p.print("\", ");
            }
        }
        r+= p.print("]");
        return r;
    }

    template<typename T> size_t print_json_value(Print& p, const char *keyname, const T &value) {
        size_t r = 0;
        r+= p.print("\"");
        r+= p.print(keyname);
        r+= p.print("\": \"");
        r+= p.print(value);
        r+= p.print("\"");
        return r;
    }

    template<typename T> size_t print_json_hexstring(Print& p, const char *keyname, const T* data, const uint16_t len) {
        size_t r = 0;
        r+= p.print("\"");
        r+= p.print(keyname);
        r+= p.print("\": \"");
        for(uint16_t i=0; i<len; i++) {
            if(data[i] < 16) {
                r+= p.print("0");
            }
            r+= _print_hex_upper(p, (uint32_t)data[i]);
        }
        r+= p.print("\"");
        return r;
    }

    template<typename T> size_t print_json_bool(Print& p, const char *keyname, const T value) {
        size_t r = 0;
        r+= p.print("\"");
        r+= p.print(keyname);
        r+= p.print("\": ");
        if(value) {
            r+= p.print("true");
        } else {
            r+= p.print("false");
        }
        r+= p.print("");
        return r;
    }

    size_t print_json_string(Print& p, const char *keyname, const char *value);
}

#endif