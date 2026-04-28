// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_CONSOLE_BACKEND_H
#define SCDK_CONSOLE_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include <scdk/types.h>

struct scdk_console_info {
    uint32_t columns;
    uint32_t rows;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t flags;
};

#define SCDK_CONSOLE_INFO_SERIAL_MIRROR 1u
#define SCDK_CONSOLE_INFO_FRAMEBUFFER_TEXT 2u

/*
 * Backend control-plane: initialize framebuffer text output plus optional
 * serial mirroring when COM1 is available.
 * User space must reach this only through console endpoint messages.
 */
scdk_status_t scdk_console_backend_init(void);

/*
 * Backend data-plane: write already-authorized text to serial and framebuffer.
 */
scdk_status_t scdk_console_backend_write(const char *buffer,
                                         size_t length);

/*
 * Backend control-plane: clear the framebuffer text surface and reset cursor.
 */
scdk_status_t scdk_console_backend_clear(void);

/*
 * Backend control-plane: move the visible framebuffer text viewport.
 * Negative lines scroll toward older output, positive lines scroll back down.
 */
scdk_status_t scdk_console_backend_scroll(int32_t lines);

/*
 * Backend diagnostic: return framebuffer text geometry and mirror flags.
 */
scdk_status_t scdk_console_backend_get_info(struct scdk_console_info *out);

/*
 * Backend diagnostic: mark/log that serial and framebuffer backends are ready.
 */
scdk_status_t scdk_console_backend_ready(void);

/*
 * Architecture audit marker. Direct serial/framebuffer writes must stay in
 * early boot, panic/log fallback, arch drivers, and console backend files.
 */
scdk_status_t scdk_console_direct_access_audit(void);

#endif
