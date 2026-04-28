// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/framebuffer.h>

#include <stddef.h>
#include <stdint.h>

#include <scdk/fb_text_console.h>

static struct limine_framebuffer *active_framebuffer;
static struct limine_framebuffer fallback_framebuffer;
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

static const uint8_t glyph_slash[7] = {
    0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10,
};

static const uint8_t glyph_greater[7] = {
    0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10,
};

static const uint8_t glyph_underscore[7] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f,
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
    case '/':
        return glyph_slash;
    case '>':
        return glyph_greater;
    case '_':
        return glyph_underscore;
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
    if (active_framebuffer == 0) {
        return 0;
    }

    if (active_framebuffer->width < 64u) {
        return 4u;
    }

    return active_framebuffer->width / 32u;
}

static uint64_t console_origin_y(void) {
    if (active_framebuffer == 0) {
        return 0;
    }

    uint64_t origin = active_framebuffer->height / 12u + 12u;

    if (origin + console_cell_height() >= active_framebuffer->height) {
        return 4u;
    }

    return origin;
}

static uint64_t console_line_step(void) {
    return console_cell_height() + 2u;
}

static uint64_t console_columns(void) {
    uint64_t origin = console_origin_x();

    if (active_framebuffer == 0 ||
        active_framebuffer->width <= origin ||
        console_cell_width() == 0u) {
        return 0;
    }

    return (active_framebuffer->width - origin) / console_cell_width();
}

static uint64_t console_rows(void) {
    uint64_t origin = console_origin_y();

    if (active_framebuffer == 0 ||
        active_framebuffer->height <= origin ||
        console_line_step() == 0u) {
        return 0;
    }

    return (active_framebuffer->height - origin) / console_line_step();
}

static void fill_rect(uint64_t x,
                      uint64_t y,
                      uint64_t width,
                      uint64_t height,
                      uint32_t pixel) {
    for (uint64_t py = 0; py < height; py++) {
        for (uint64_t px = 0; px < width; px++) {
            put_pixel(x + px, y + py, pixel);
        }
    }
}

static void clear_console_cell(uint64_t x, uint64_t y) {
    uint32_t background = pack_rgb(8u, 13u, 18u);
    uint64_t width = console_cell_width();
    uint64_t height = console_cell_height();

    fill_rect(x, y, width, height, background);
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

static void scroll_console(void) {
    uint64_t origin_x = console_origin_x();
    uint64_t origin_y = console_origin_y();
    uint64_t line_step = console_line_step();
    uint64_t bottom_y;
    uint32_t background = pack_rgb(8u, 13u, 18u);

    if (active_framebuffer == 0 || line_step == 0u) {
        return;
    }

    bottom_y = active_framebuffer->height;
    if (bottom_y <= origin_y + line_step) {
        return;
    }

    for (uint64_t y = origin_y; y + line_step < bottom_y; y++) {
        uint8_t *dst = (uint8_t *)active_framebuffer->address + y * active_framebuffer->pitch;
        uint8_t *src = (uint8_t *)active_framebuffer->address + (y + line_step) * active_framebuffer->pitch;
        for (uint64_t x = origin_x * 4u; x < active_framebuffer->pitch; x++) {
            dst[x] = src[x];
        }
    }

    fill_rect(origin_x,
              bottom_y - line_step,
              active_framebuffer->width - origin_x,
              line_step,
              background);
}

static void console_newline(void) {
    uint64_t line_step = console_line_step();

    console_cursor_x = console_origin_x();
    console_cursor_y += line_step;

    if (active_framebuffer != 0 &&
        console_cursor_y + console_cell_height() >= active_framebuffer->height) {
        scroll_console();
        console_cursor_y = active_framebuffer->height - line_step;
    }
}

static void console_carriage_return(void) {
    console_cursor_x = console_origin_x();
}

static void console_backspace(void) {
    uint64_t origin = console_origin_x();

    if (console_cursor_x <= origin) {
        return;
    }

    console_cursor_x -= console_cell_width();
    clear_console_cell(console_cursor_x, console_cursor_y);
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

scdk_status_t scdk_fb_text_init(void *framebuffer,
                                uint32_t width,
                                uint32_t height,
                                uint32_t pitch,
                                uint32_t bpp) {
    if (framebuffer == 0 || width == 0u || height == 0u || pitch == 0u || bpp != 32u) {
        return SCDK_ERR_INVAL;
    }

    if (active_framebuffer == 0 || active_framebuffer->address != framebuffer) {
        fallback_framebuffer.address = framebuffer;
        fallback_framebuffer.width = width;
        fallback_framebuffer.height = height;
        fallback_framebuffer.pitch = pitch;
        fallback_framebuffer.bpp = bpp;
        fallback_framebuffer.memory_model = 1;
        fallback_framebuffer.red_mask_size = 8;
        fallback_framebuffer.red_mask_shift = 16;
        fallback_framebuffer.green_mask_size = 8;
        fallback_framebuffer.green_mask_shift = 8;
        fallback_framebuffer.blue_mask_size = 8;
        fallback_framebuffer.blue_mask_shift = 0;
        active_framebuffer = &fallback_framebuffer;
    }

    console_cursor_ready = false;
    return scdk_fb_text_clear();
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
    (void)scdk_fb_text_write(s, len);
}

scdk_status_t scdk_fb_text_write(const char *buf, size_t len) {
    if (buf == 0) {
        return SCDK_ERR_INVAL;
    }

    if (len == 0u) {
        while (buf[len] != '\0') {
            len++;
        }
    }

    for (size_t i = 0; i < len; i++) {
        scdk_status_t status = scdk_fb_text_putchar(buf[i]);
        if (status != SCDK_OK) {
            return status;
        }
    }

    return SCDK_OK;
}

scdk_status_t scdk_fb_text_putchar(char ch) {
    if (active_framebuffer == 0 || active_framebuffer->bpp != 32) {
        return SCDK_ERR_NOTSUP;
    }

    ensure_console_cursor();

    if (ch == '\n') {
        console_newline();
        return SCDK_OK;
    }

    if (ch == '\r') {
        console_carriage_return();
        return SCDK_OK;
    }

    if (ch == '\b') {
        console_backspace();
        return SCDK_OK;
    }

    if (ch == '\t') {
        do {
            scdk_status_t status = scdk_fb_text_putchar(' ');
            if (status != SCDK_OK) {
                return status;
            }
        } while (((console_cursor_x - console_origin_x()) / console_cell_width()) % 4u != 0u);
        return SCDK_OK;
    }

    if (console_cursor_x + console_cell_width() >= active_framebuffer->width) {
        console_newline();
    }

    draw_console_char(ch, console_cursor_x, console_cursor_y);
    console_cursor_x += console_cell_width();
    return SCDK_OK;
}

scdk_status_t scdk_fb_text_clear(void) {
    uint32_t background;

    if (active_framebuffer == 0 || active_framebuffer->bpp != 32) {
        return SCDK_ERR_NOTSUP;
    }

    background = pack_rgb(8u, 13u, 18u);
    fill_rect(0,
              0,
              active_framebuffer->width,
              active_framebuffer->height,
              background);
    console_cursor_x = console_origin_x();
    console_cursor_y = console_origin_y();
    console_cursor_ready = true;
    return SCDK_OK;
}

scdk_status_t scdk_fb_text_set_cursor(uint32_t x, uint32_t y) {
    uint64_t columns = console_columns();
    uint64_t rows = console_rows();

    if (active_framebuffer == 0 || columns == 0u || rows == 0u) {
        return SCDK_ERR_NOTSUP;
    }

    if (x >= columns || y >= rows) {
        return SCDK_ERR_BOUNDS;
    }

    console_cursor_x = console_origin_x() + (uint64_t)x * console_cell_width();
    console_cursor_y = console_origin_y() + (uint64_t)y * console_line_step();
    console_cursor_ready = true;
    return SCDK_OK;
}

scdk_status_t scdk_fb_text_get_info(struct scdk_console_info *out) {
    if (out == 0) {
        return SCDK_ERR_INVAL;
    }

    if (active_framebuffer == 0 || active_framebuffer->bpp != 32) {
        return SCDK_ERR_NOTSUP;
    }

    ensure_console_cursor();

    out->columns = (uint32_t)console_columns();
    out->rows = (uint32_t)console_rows();
    out->cursor_x = (uint32_t)((console_cursor_x - console_origin_x()) / console_cell_width());
    out->cursor_y = (uint32_t)((console_cursor_y - console_origin_y()) / console_line_step());
    out->flags = SCDK_CONSOLE_INFO_FRAMEBUFFER_TEXT;
    return SCDK_OK;
}
