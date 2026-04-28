// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_EARLY_CONSOLE_H
#define SCDK_EARLY_CONSOLE_H

#include <stdbool.h>

/*
 * Early boot path: probe COM1 before the normal console endpoint exists.
 * Serial is an optional debug sink; failure must not block normal boot.
 */
bool scdk_early_console_init(void);

/*
 * Early boot diagnostic: record that framebuffer-backed panic output can be
 * used once Limine framebuffer setup has completed.
 */
void scdk_early_console_framebuffer_ready(void);

#endif
