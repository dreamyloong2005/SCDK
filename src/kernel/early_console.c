// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/early_console.h>

#include <scdk/serial.h>

static bool early_console_serial_ready;
static bool early_console_framebuffer_ready;

bool scdk_early_console_init(void) {
    early_console_serial_ready = scdk_serial_init();
    if (early_console_serial_ready) {
        (void)scdk_serial_write_string("[console] early serial ok\n");
    }

    return early_console_serial_ready;
}

void scdk_early_console_framebuffer_ready(void) {
    early_console_framebuffer_ready = true;
    (void)early_console_framebuffer_ready;

    if (early_console_serial_ready) {
        (void)scdk_serial_write_string("[early-console] framebuffer panic path available\n");
    }
}
