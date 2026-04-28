// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_INPUT_H
#define SCDK_INPUT_H

#include <stdint.h>

enum scdk_input_event_type {
    SCDK_INPUT_NONE = 0,
    SCDK_INPUT_KEY_DOWN,
    SCDK_INPUT_KEY_UP,
};

enum scdk_input_modifiers {
    SCDK_INPUT_MOD_SHIFT = 1u << 0,
};

enum scdk_keycode {
    SCDK_KEY_NONE = 0,
    SCDK_KEY_ENTER = 13,
    SCDK_KEY_BACKSPACE = 8,
    SCDK_KEY_TAB = 9,
    SCDK_KEY_ESCAPE = 27,
};

struct scdk_input_event {
    uint64_t timestamp;
    uint32_t type;
    uint32_t keycode;
    uint32_t ascii;
    uint32_t modifiers;
    uint32_t flags;
};

#endif
