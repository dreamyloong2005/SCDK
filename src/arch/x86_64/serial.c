// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/serial.h>

#include <scdk/status.h>

#include <stdint.h>

#define COM1_PORT 0x3f8u
#define SERIAL_TRANSMIT_SPINS 100000u

static bool serial_ready;

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
    serial_ready = false;

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
    serial_ready = true;
    return true;
}

bool scdk_serial_is_available(void) {
    return serial_ready;
}

static scdk_status_t serial_write_byte(uint8_t value) {
    if (!serial_ready) {
        return SCDK_ERR_NOTSUP;
    }

    for (uint32_t spin = 0; spin < SERIAL_TRANSMIT_SPINS; spin++) {
        if (transmit_empty()) {
            outb(COM1_PORT, value);
            return SCDK_OK;
        }

        __asm__ volatile ("pause");
    }

    serial_ready = false;
    return SCDK_ERR_BUSY;
}

scdk_status_t scdk_serial_write_char(char c) {
    scdk_status_t status;

    if (c == '\n') {
        status = serial_write_byte((uint8_t)'\r');
        if (status != SCDK_OK) {
            return status;
        }
    }

    return serial_write_byte((uint8_t)c);
}

scdk_status_t scdk_serial_write_string(const char *s) {
    scdk_status_t final_status = SCDK_OK;

    if (s == 0) {
        s = "(null)";
    }

    while (*s != '\0') {
        scdk_status_t status = scdk_serial_write_char(*s++);
        if (status != SCDK_OK) {
            final_status = status;
            break;
        }
    }

    return final_status;
}
