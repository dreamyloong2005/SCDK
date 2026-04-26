// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/serial.h>

#include <stdint.h>

#define COM1_PORT 0x3f8u

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

static bool transmit_empty(void) {
    return (inb(COM1_PORT + 5u) & 0x20u) != 0;
}

bool scdk_serial_init(void) {
    outb(COM1_PORT + 1u, 0x00u);
    outb(COM1_PORT + 3u, 0x80u);
    outb(COM1_PORT + 0u, 0x01u);
    outb(COM1_PORT + 1u, 0x00u);
    outb(COM1_PORT + 3u, 0x03u);
    outb(COM1_PORT + 2u, 0xc7u);
    outb(COM1_PORT + 4u, 0x0bu);
    outb(COM1_PORT + 4u, 0x1eu);
    outb(COM1_PORT + 0u, 0xaeu);

    if (inb(COM1_PORT + 0u) != 0xaeu) {
        return false;
    }

    outb(COM1_PORT + 4u, 0x0fu);
    return true;
}

void scdk_serial_write_char(char c) {
    if (c == '\n') {
        scdk_serial_write_char('\r');
    }

    while (!transmit_empty()) {
        __asm__ volatile ("pause");
    }

    outb(COM1_PORT, (uint8_t)c);
}

void scdk_serial_write_string(const char *s) {
    if (s == 0) {
        s = "(null)";
    }

    while (*s != '\0') {
        scdk_serial_write_char(*s++);
    }
}
