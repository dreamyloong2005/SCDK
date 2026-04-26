// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_FRAMEBUFFER_H
#define SCDK_FRAMEBUFFER_H

#include <stdbool.h>
#include <stddef.h>

#include <limine.h>

bool scdk_framebuffer_init(struct limine_framebuffer *framebuffer);
void scdk_framebuffer_draw_test_pattern(void);
void scdk_framebuffer_console_write(const char *s, size_t len);

#endif
