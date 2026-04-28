// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_KEYBOARD_H
#define SCDK_KEYBOARD_H

#include <scdk/input.h>
#include <scdk/types.h>

/*
 * Hardware backend: initialize the PS/2 keyboard polling path.
 */
scdk_status_t scdk_keyboard_init(void);

/*
 * Input data-plane: return one queued or newly-polled keyboard event.
 */
scdk_status_t scdk_keyboard_poll(struct scdk_input_event *out);

/*
 * Boot self-test hook: inject one key event into the same queue used by the
 * polling backend. Not a userspace ABI.
 */
scdk_status_t scdk_keyboard_inject_test_key(uint32_t ascii);

#endif
