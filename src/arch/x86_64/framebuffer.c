// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/framebuffer.h>

#include <stddef.h>
#include <stdint.h>

static struct limine_framebuffer *active_framebuffer;
static uint64_t console_cursor_x;
static uint64_t console_cursor_y;
static bool console_cursor_ready;

static uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
    if (active_framebuffer == 0) {
        return 0;
    }

    uint32_t pixel = 0;
    pixel |= ((uint32_t)r >> (8u - active_framebuffer->red_mask_size))
        << active_framebuffer->red_mask_shift;
    pixel |= ((uint32_t)g >> (8u - active_framebuffer->green_mask_size))
        << active_framebuffer->green_mask_shift;
    pixel |= ((uint32_t)b >> (8u - active_framebuffer->blue_mask_size))
        << active_framebuffer->blue_mask_shift;
    return pixel;
}

static void put_pixel(uint64_t x, uint64_t y, uint32_t pixel) {
    if (active_framebuffer == 0 || active_framebuffer->bpp != 32) {
        return;
    }

    if (x >= active_framebuffer->width || y >= active_framebuffer->height) {
        return;
    }

    uint8_t *row = (uint8_t *)active_framebuffer->address + y * active_framebuffer->pitch;
    ((uint32_t *)row)[x] = pixel;
}

static const uint8_t glyph_blank[7] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t glyph_unknown[7] = {
    0x1f, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04,
};

static const uint8_t glyph_letters[26][7] = {
    { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },
    { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e },
    { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e },
    { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e },
    { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f },
    { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 },
    { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f },
    { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },
    { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e },
    { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c },
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 },
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f },
    { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 },
    { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 },
    { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },
    { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 },
    { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d },
    { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 },
    { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e },
    { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },
    { 0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04 },
    { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11 },
    { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 },
    { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 },
    { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f },
};

static const uint8_t glyph_digits[10][7] = {
    { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e },
    { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e },
    { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f },
    { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e },
    { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 },
    { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e },
    { 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e },
    { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
    { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e },
    { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e },
};

static const uint8_t glyph_colon[7] = {
    0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00,
};

static const uint8_t glyph_dash[7] = {
    0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00,
};

static const uint8_t glyph_dot[7] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c,
};

static const uint8_t glyph_left_bracket[7] = {
    0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e,
};

static const uint8_t glyph_right_bracket[7] = {
    0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e,
};

static const uint8_t *glyph_for(char c) {
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    if (c >= 'A' && c <= 'Z') {
        return glyph_letters[(uint32_t)(c - 'A')];
    }

    if (c >= '0' && c <= '9') {
        return glyph_digits[(uint32_t)(c - '0')];
    }

    switch (c) {
    case ' ':
        return glyph_blank;
    case ':':
        return glyph_colon;
    case '-':
        return glyph_dash;
    case '.':
        return glyph_dot;
    case '[':
        return glyph_left_bracket;
    case ']':
        return glyph_right_bracket;
    default:
        return glyph_unknown;
    }
}

static uint64_t console_cell_width(void) {
    return 14u;
}

static uint64_t console_cell_height(void) {
    return 20u;
}

static uint64_t console_origin_x(void) {
    if (active_framebuffer->width < 64u) {
        return 4u;
    }

    return active_framebuffer->width / 32u;
}

static uint64_t console_origin_y(void) {
    uint64_t origin = active_framebuffer->height / 12u + 12u;

    if (origin + console_cell_height() >= active_framebuffer->height) {
        return 4u;
    }

    return origin;
}

static void clear_console_cell(uint64_t x, uint64_t y) {
    uint32_t background = pack_rgb(8u, 13u, 18u);
    uint64_t width = console_cell_width();
    uint64_t height = console_cell_height();

    for (uint64_t py = 0; py < height; py++) {
        for (uint64_t px = 0; px < width; px++) {
            put_pixel(x + px, y + py, background);
        }
    }
}

static void draw_console_char(char c, uint64_t x, uint64_t y) {
    const uint8_t *glyph = glyph_for(c);
    uint32_t foreground = pack_rgb(226u, 232u, 240u);
    uint32_t shadow = pack_rgb(15u, 23u, 42u);
    uint64_t scale = 2u;

    clear_console_cell(x, y);

    for (uint64_t row = 0; row < 7u; row++) {
        for (uint64_t col = 0; col < 5u; col++) {
            if ((glyph[row] & (1u << (4u - col))) == 0u) {
                continue;
            }

            uint64_t ox = x + 2u + col * scale;
            uint64_t oy = y + 3u + row * scale;
            for (uint64_t sy = 0; sy < scale; sy++) {
                for (uint64_t sx = 0; sx < scale; sx++) {
                    put_pixel(ox + sx + 1u, oy + sy + 1u, shadow);
                    put_pixel(ox + sx, oy + sy, foreground);
                }
            }
        }
    }
}

static void console_newline(void) {
    console_cursor_x = console_origin_x();
    console_cursor_y += console_cell_height() + 2u;

    if (console_cursor_y + console_cell_height() >= active_framebuffer->height) {
        console_cursor_y = console_origin_y();
    }
}

static void ensure_console_cursor(void) {
    if (console_cursor_ready) {
        return;
    }

    console_cursor_x = console_origin_x();
    console_cursor_y = console_origin_y();
    console_cursor_ready = true;
}

bool scdk_framebuffer_init(struct limine_framebuffer *framebuffer) {
    if (framebuffer == 0 || framebuffer->address == 0) {
        return false;
    }

    active_framebuffer = framebuffer;
    console_cursor_ready = false;
    return true;
}

void scdk_framebuffer_draw_test_pattern(void) {
    if (active_framebuffer == 0 || active_framebuffer->bpp != 32) {
        return;
    }

    uint64_t width = active_framebuffer->width;
    uint64_t height = active_framebuffer->height;

    for (uint64_t y = 0; y < height; y++) {
        for (uint64_t x = 0; x < width; x++) {
            uint8_t r = (uint8_t)((x * 255u) / (width == 0 ? 1u : width));
            uint8_t g = (uint8_t)((y * 255u) / (height == 0 ? 1u : height));
            uint8_t b = (uint8_t)(((x + y) * 127u) / ((width + height) == 0 ? 1u : (width + height)));
            put_pixel(x, y, pack_rgb(r, g, b));
        }
    }

    uint64_t bar_height = height / 12u;
    if (bar_height < 24u) {
        bar_height = 24u;
    }

    uint32_t dark = pack_rgb(12u, 18u, 24u);
    uint32_t cyan = pack_rgb(56u, 189u, 248u);
    uint32_t green = pack_rgb(34u, 197u, 94u);

    for (uint64_t y = 0; y < bar_height && y < height; y++) {
        for (uint64_t x = 0; x < width; x++) {
            put_pixel(x, y, dark);
        }
    }

    uint64_t inset = width / 32u;
    uint64_t line_y = bar_height / 2u;
    uint64_t line_h = 4u;
    for (uint64_t y = line_y; y < line_y + line_h && y < height; y++) {
        for (uint64_t x = inset; x + inset < width; x++) {
            put_pixel(x, y, (x & 16u) == 0u ? cyan : green);
        }
    }
}

void scdk_framebuffer_console_write(const char *s, size_t len) {
    if (active_framebuffer == 0 || active_framebuffer->bpp != 32 || s == 0) {
        return;
    }

    ensure_console_cursor();

    for (size_t i = 0; i < len; i++) {
        char c = s[i];

        if (c == '\n') {
            console_newline();
            continue;
        }

        if (console_cursor_x + console_cell_width() >= active_framebuffer->width) {
            console_newline();
        }

        draw_console_char(c, console_cursor_x, console_cursor_y);
        console_cursor_x += console_cell_width();
    }
}
