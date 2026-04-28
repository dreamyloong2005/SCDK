// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_FB_TEXT_CONSOLE_H
#define SCDK_FB_TEXT_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

#include <scdk/console_backend.h>
#include <scdk/types.h>

scdk_status_t scdk_fb_text_init(void *framebuffer,
                                uint32_t width,
                                uint32_t height,
                                uint32_t pitch,
                                uint32_t bpp);
scdk_status_t scdk_fb_text_write(const char *buf, size_t len);
scdk_status_t scdk_fb_text_putchar(char ch);
scdk_status_t scdk_fb_text_clear(void);
scdk_status_t scdk_fb_text_set_cursor(uint32_t x, uint32_t y);
scdk_status_t scdk_fb_text_get_info(struct scdk_console_info *out);

#endif
