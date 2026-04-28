// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/keyboard.h>

#include <stdbool.h>
#include <stdint.h>

#define PS2_DATA_PORT 0x60u
#define PS2_STATUS_PORT 0x64u
#define PS2_STATUS_OUTPUT_FULL 0x01u

#define KEY_QUEUE_SIZE 32u

struct key_map_entry {
    uint32_t keycode;
    char normal;
    char shifted;
};

static const struct key_map_entry scancode_set1[128] = {
    [0x01] = { SCDK_KEY_ESCAPE, 27, 27 },
    [0x02] = { '1', '1', '!' },
    [0x03] = { '2', '2', '@' },
    [0x04] = { '3', '3', '#' },
    [0x05] = { '4', '4', '$' },
    [0x06] = { '5', '5', '%' },
    [0x07] = { '6', '6', '^' },
    [0x08] = { '7', '7', '&' },
    [0x09] = { '8', '8', '*' },
    [0x0a] = { '9', '9', '(' },
    [0x0b] = { '0', '0', ')' },
    [0x0c] = { '-', '-', '_' },
    [0x0d] = { '=', '=', '+' },
    [0x0e] = { SCDK_KEY_BACKSPACE, '\b', '\b' },
    [0x0f] = { SCDK_KEY_TAB, '\t', '\t' },
    [0x10] = { 'q', 'q', 'Q' },
    [0x11] = { 'w', 'w', 'W' },
    [0x12] = { 'e', 'e', 'E' },
    [0x13] = { 'r', 'r', 'R' },
    [0x14] = { 't', 't', 'T' },
    [0x15] = { 'y', 'y', 'Y' },
    [0x16] = { 'u', 'u', 'U' },
    [0x17] = { 'i', 'i', 'I' },
    [0x18] = { 'o', 'o', 'O' },
    [0x19] = { 'p', 'p', 'P' },
    [0x1a] = { '[', '[', '{' },
    [0x1b] = { ']', ']', '}' },
    [0x1c] = { SCDK_KEY_ENTER, '\n', '\n' },
    [0x1e] = { 'a', 'a', 'A' },
    [0x1f] = { 's', 's', 'S' },
    [0x20] = { 'd', 'd', 'D' },
    [0x21] = { 'f', 'f', 'F' },
    [0x22] = { 'g', 'g', 'G' },
    [0x23] = { 'h', 'h', 'H' },
    [0x24] = { 'j', 'j', 'J' },
    [0x25] = { 'k', 'k', 'K' },
    [0x26] = { 'l', 'l', 'L' },
    [0x27] = { ';', ';', ':' },
    [0x28] = { '\'', '\'', '"' },
    [0x29] = { '`', '`', '~' },
    [0x2b] = { '\\', '\\', '|' },
    [0x2c] = { 'z', 'z', 'Z' },
    [0x2d] = { 'x', 'x', 'X' },
    [0x2e] = { 'c', 'c', 'C' },
    [0x2f] = { 'v', 'v', 'V' },
    [0x30] = { 'b', 'b', 'B' },
    [0x31] = { 'n', 'n', 'N' },
    [0x32] = { 'm', 'm', 'M' },
    [0x33] = { ',', ',', '<' },
    [0x34] = { '.', '.', '>' },
    [0x35] = { '/', '/', '?' },
    [0x39] = { ' ', ' ', ' ' },
    [0x3a] = { SCDK_KEY_CAPS_LOCK, 0, 0 },
};

static const struct key_map_entry extended_scancode_set1[128] = {
    [0x47] = { SCDK_KEY_HOME, 0, 0 },
    [0x48] = { SCDK_KEY_UP, 0, 0 },
    [0x49] = { SCDK_KEY_PAGE_UP, 0, 0 },
    [0x4b] = { SCDK_KEY_LEFT, 0, 0 },
    [0x4d] = { SCDK_KEY_RIGHT, 0, 0 },
    [0x4f] = { SCDK_KEY_END, 0, 0 },
    [0x50] = { SCDK_KEY_DOWN, 0, 0 },
    [0x51] = { SCDK_KEY_PAGE_DOWN, 0, 0 },
};

static struct scdk_input_event queue[KEY_QUEUE_SIZE];
static uint32_t queue_head;
static uint32_t queue_tail;
static uint32_t queue_count;
static uint64_t event_clock;
static bool keyboard_initialized;
static bool left_shift_down;
static bool right_shift_down;
static bool caps_lock_on;
static bool extended_scancode_pending;

static bool is_alpha_entry(const struct key_map_entry *entry) {
    return entry != 0 &&
           entry->normal >= 'a' &&
           entry->normal <= 'z' &&
           entry->shifted >= 'A' &&
           entry->shifted <= 'Z';
}

static bool shift_down(void) {
    return left_shift_down || right_shift_down;
}

static uint32_t current_modifiers(void) {
    uint32_t modifiers = 0;

    if (shift_down()) {
        modifiers |= SCDK_INPUT_MOD_SHIFT;
    }
    if (caps_lock_on) {
        modifiers |= SCDK_INPUT_MOD_CAPS_LOCK;
    }

    return modifiers;
}

static uint32_t ascii_for_entry(const struct key_map_entry *entry) {
    bool shift = shift_down();

    if (entry == 0) {
        return 0;
    }

    if (is_alpha_entry(entry)) {
        return (uint32_t)(uint8_t)((shift != caps_lock_on) ? entry->shifted : entry->normal);
    }

    return (uint32_t)(uint8_t)(shift ? entry->shifted : entry->normal);
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

static scdk_status_t enqueue_event(const struct scdk_input_event *event) {
    if (event == 0) {
        return SCDK_ERR_INVAL;
    }

    if (queue_count >= KEY_QUEUE_SIZE) {
        return SCDK_ERR_BUSY;
    }

    queue[queue_tail] = *event;
    queue_tail = (queue_tail + 1u) % KEY_QUEUE_SIZE;
    queue_count++;
    return SCDK_OK;
}

static scdk_status_t dequeue_event(struct scdk_input_event *out) {
    if (out == 0) {
        return SCDK_ERR_INVAL;
    }

    if (queue_count == 0u) {
        return SCDK_ERR_NOENT;
    }

    *out = queue[queue_head];
    queue_head = (queue_head + 1u) % KEY_QUEUE_SIZE;
    queue_count--;
    return SCDK_OK;
}

static scdk_status_t handle_scancode(uint8_t scancode) {
    struct scdk_input_event event = { 0 };
    bool extended;
    bool released;
    uint8_t code;
    const struct key_map_entry *entry;

    if (scancode == 0xe0u) {
        extended_scancode_pending = true;
        return SCDK_ERR_NOENT;
    }

    extended = extended_scancode_pending;
    extended_scancode_pending = false;
    released = (scancode & 0x80u) != 0u;
    code = scancode & 0x7fu;

    if (!extended && code == 0x2au) {
        left_shift_down = !released;
        return SCDK_ERR_NOENT;
    }

    if (!extended && code == 0x36u) {
        right_shift_down = !released;
        return SCDK_ERR_NOENT;
    }

    if (code >= 128u) {
        return SCDK_ERR_NOENT;
    }

    entry = extended ? &extended_scancode_set1[code] : &scancode_set1[code];
    if (entry->keycode == 0u) {
        return SCDK_ERR_NOENT;
    }

    if (entry->keycode == SCDK_KEY_CAPS_LOCK && !released) {
        caps_lock_on = !caps_lock_on;
    }

    event.timestamp = ++event_clock;
    event.type = released ? SCDK_INPUT_KEY_UP : SCDK_INPUT_KEY_DOWN;
    event.keycode = entry->keycode;
    event.ascii = released ? 0u : ascii_for_entry(entry);
    event.modifiers = current_modifiers();
    event.flags = 0;
    return enqueue_event(&event);
}

scdk_status_t scdk_keyboard_init(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    event_clock = 0;
    left_shift_down = false;
    right_shift_down = false;
    caps_lock_on = false;
    extended_scancode_pending = false;
    keyboard_initialized = true;
    return SCDK_OK;
}

scdk_status_t scdk_keyboard_poll(struct scdk_input_event *out) {
    scdk_status_t status;

    if (out == 0) {
        return SCDK_ERR_INVAL;
    }

    if (!keyboard_initialized) {
        status = scdk_keyboard_init();
        if (status != SCDK_OK) {
            return status;
        }
    }

    status = dequeue_event(out);
    if (status == SCDK_OK) {
        return SCDK_OK;
    }

    if ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) == 0u) {
        return SCDK_ERR_NOENT;
    }

    (void)handle_scancode(inb(PS2_DATA_PORT));
    return dequeue_event(out);
}

scdk_status_t scdk_keyboard_inject_test_key(uint32_t ascii) {
    struct scdk_input_event event = { 0 };

    if (!keyboard_initialized) {
        scdk_status_t status = scdk_keyboard_init();
        if (status != SCDK_OK) {
            return status;
        }
    }

    if (ascii == 0u || ascii > 0x7fu) {
        return SCDK_ERR_INVAL;
    }

    event.timestamp = ++event_clock;
    event.type = SCDK_INPUT_KEY_DOWN;
    event.keycode = ascii;
    event.ascii = ascii;
    event.modifiers = 0;
    event.flags = 0;
    return enqueue_event(&event);
}
