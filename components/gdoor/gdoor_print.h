/*
 * Minimal Arduino-compatible Print / Printable replacement for pure ESP-IDF builds.
 *
 * When ESPHome is built WITH the Arduino framework, Arduino.h is already included
 * transitively (via esphome/core/macros.h) and Arduino's own Print/Printable are
 * available — this file becomes a no-op.
 *
 * When ESPHome is built WITHOUT the Arduino framework (pure ESP-IDF), Arduino.h
 * is absent. This file provides the subset of Print/Printable that this component
 * uses, implemented in standard C++ with no Arduino dependencies.
 *
 * Drop-in: include this instead of Arduino.h wherever Print/Printable are needed.
 */
#pragma once

#ifdef ARDUINO

// Arduino framework is present — Print/Printable already defined in Arduino.h.
// Pull in Arduino.h so HEX/DEC and all integer types are available.
#include <Arduino.h>

#else  // pure ESP-IDF build

#include <stdint.h>
#include <stddef.h>
#include <string.h>   // strlen

// Keep the same macro names Arduino uses so existing call-sites need no change.
#ifndef HEX
#define HEX 16
#endif
#ifndef DEC
#define DEC 10
#endif

// ---------------------------------------------------------------------------
// Print — abstract base class; subclasses implement write(uint8_t).
// Provides print() overloads for strings and integer types.
// ---------------------------------------------------------------------------
class Print {
public:
    virtual ~Print() = default;

    // --- pure virtual byte sink ---
    virtual size_t write(uint8_t c) = 0;

    // --- string helpers ---
    size_t write(const char *s) {
        if (!s) return 0;
        size_t n = 0;
        while (*s) n += write((uint8_t)*s++);
        return n;
    }
    size_t print(const char *s) { return write(s); }

    // --- integer helpers ---
    size_t print(uint8_t  v, int base = DEC) { return _num((uint32_t)v, base); }
    size_t print(uint16_t v, int base = DEC) { return _num((uint32_t)v, base); }
    size_t print(uint32_t v, int base = DEC) { return _num((uint32_t)v, base); }
    size_t print(int      v, int base = DEC) {
        if (base == DEC && v < 0) {
            size_t n = write((uint8_t)'-');
            return n + _num((uint32_t)(-v), base);
        }
        return _num((uint32_t)v, base);
    }
    // Note: on ESP32, unsigned long == uint32_t and long == int (both 32-bit).
    // Separate overloads would be duplicate declarations and are therefore omitted.

private:
    size_t _num(uint32_t value, int base) {
        // Build digits right-to-left in a local buffer.
        char buf[33];
        char *p = buf + sizeof(buf) - 1;
        *p = '\0';
        if (value == 0) {
            *--p = '0';
        } else {
            while (value > 0) {
                int d = (int)(value % (uint32_t)base);
                *--p = (char)(d < 10 ? ('0' + d) : ('A' + d - 10));
                value /= (uint32_t)base;
            }
        }
        return write(p);
    }
};

// ---------------------------------------------------------------------------
// Printable — interface for objects that can serialise themselves to a Print.
// ---------------------------------------------------------------------------
class Printable {
public:
    virtual ~Printable() = default;
    virtual size_t printTo(Print &p) const = 0;
};

#endif  // ARDUINO
