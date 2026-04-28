// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/console_backend.h>

#include <stdbool.h>
#include <stddef.h>

#include <scdk/fb_text_console.h>
#include <scdk/serial.h>

static bool console_backend_initialized;
static bool serial_mirror_enabled;

scdk_status_t scdk_console_backend_init(void) {
    console_backend_initialized = true;
    serial_mirror_enabled = scdk_serial_is_available();
    (void)scdk_fb_text_clear();
    return SCDK_OK;
}

scdk_status_t scdk_console_backend_write(const char *buffer,
                                         size_t length) {
    bool needs_newline;

    if (buffer == 0) {
        return SCDK_ERR_INVAL;
    }

    if (length == 0u) {
        while (buffer[length] != '\0') {
            length++;
        }
    }

    needs_newline = length == 0u || buffer[length - 1u] != '\n';

    if (!console_backend_initialized) {
        (void)scdk_console_backend_init();
    }

    serial_mirror_enabled = scdk_serial_is_available();

    if (serial_mirror_enabled) {
        for (size_t i = 0; i < length; i++) {
            if (scdk_serial_write_char(buffer[i]) != SCDK_OK) {
                serial_mirror_enabled = false;
                break;
            }
        }

        if (serial_mirror_enabled && needs_newline) {
            if (scdk_serial_write_char('\n') != SCDK_OK) {
                serial_mirror_enabled = false;
            }
        }
    }

    (void)scdk_fb_text_write(buffer, length);
    if (needs_newline) {
        (void)scdk_fb_text_putchar('\n');
    }

    return SCDK_OK;
}

scdk_status_t scdk_console_backend_clear(void) {
    scdk_status_t status;

    if (!console_backend_initialized) {
        status = scdk_console_backend_init();
        if (status != SCDK_OK) {
            return status;
        }
    }

    return scdk_fb_text_clear();
}

scdk_status_t scdk_console_backend_get_info(struct scdk_console_info *out) {
    scdk_status_t status;

    if (out == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_fb_text_get_info(out);
    if (status != SCDK_OK) {
        return status;
    }

    serial_mirror_enabled = scdk_serial_is_available();
    if (serial_mirror_enabled) {
        out->flags |= SCDK_CONSOLE_INFO_SERIAL_MIRROR;
    }

    return SCDK_OK;
}

scdk_status_t scdk_console_backend_ready(void) {
    return scdk_console_backend_init();
}

scdk_status_t scdk_console_direct_access_audit(void) {
    return SCDK_OK;
}
