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
};

static struct scdk_input_event queue[KEY_QUEUE_SIZE];
static uint32_t queue_head;
static uint32_t queue_tail;
static uint32_t queue_count;
static uint64_t event_clock;
static bool keyboard_initialized;
static bool shift_down;

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
    bool released = (scancode & 0x80u) != 0u;
    uint8_t code = scancode & 0x7fu;
    const struct key_map_entry *entry;

    if (code == 0x2au || code == 0x36u) {
        shift_down = !released;
        return SCDK_ERR_NOENT;
    }

    if (code >= 128u) {
        return SCDK_ERR_NOENT;
    }

    entry = &scancode_set1[code];
    if (entry->keycode == 0u) {
        return SCDK_ERR_NOENT;
    }

    event.timestamp = ++event_clock;
    event.type = released ? SCDK_INPUT_KEY_UP : SCDK_INPUT_KEY_DOWN;
    event.keycode = entry->keycode;
    event.ascii = released ? 0u : (uint32_t)(uint8_t)(shift_down ? entry->shifted : entry->normal);
    event.modifiers = shift_down ? SCDK_INPUT_MOD_SHIFT : 0u;
    event.flags = 0;
    return enqueue_event(&event);
}

scdk_status_t scdk_keyboard_init(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    event_clock = 0;
    shift_down = false;
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
