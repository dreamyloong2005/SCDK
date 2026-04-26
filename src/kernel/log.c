// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/log.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/message.h>
#include <scdk/serial.h>

#define SCDK_LOG_BUFFER_SIZE 512u

struct scdk_log_buffer {
    char data[SCDK_LOG_BUFFER_SIZE];
    size_t len;
};

static scdk_cap_t log_console_endpoint;
static bool log_dispatching;

static void serial_write_string(const char *s) {
    scdk_serial_write_string(s);
}

static void append_char(struct scdk_log_buffer *buffer, char c) {
    if (buffer->len + 1u >= sizeof(buffer->data)) {
        return;
    }

    buffer->data[buffer->len++] = c;
    buffer->data[buffer->len] = '\0';
}

static void append_string(struct scdk_log_buffer *buffer, const char *s) {
    if (s == 0) {
        s = "(null)";
    }

    while (*s != '\0') {
        append_char(buffer, *s++);
    }
}

static void append_unsigned(struct scdk_log_buffer *buffer,
                            uint64_t value,
                            uint32_t base,
                            bool upper) {
    char buf[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t i = 0;

    if (value == 0) {
        append_char(buffer, '0');
        return;
    }

    while (value != 0 && i < sizeof(buf)) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (i > 0) {
        append_char(buffer, buf[--i]);
    }
}

static void append_signed(struct scdk_log_buffer *buffer, int64_t value) {
    if (value < 0) {
        append_char(buffer, '-');
        append_unsigned(buffer, (uint64_t)(-(value + 1)) + 1u, 10u, false);
        return;
    }

    append_unsigned(buffer, (uint64_t)value, 10u, false);
}

static void append_format(struct scdk_log_buffer *buffer,
                          const char *fmt,
                          va_list args) {
    while (*fmt != '\0') {
        if (*fmt != '%') {
            append_char(buffer, *fmt++);
            continue;
        }

        fmt++;

        if (*fmt == '%') {
            append_char(buffer, '%');
            fmt++;
            continue;
        }

        bool long_arg = false;
        bool long_long_arg = false;

        if (*fmt == 'l') {
            long_arg = true;
            fmt++;
            if (*fmt == 'l') {
                long_long_arg = true;
                fmt++;
            }
        } else if (*fmt == 'z') {
            long_arg = true;
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            append_char(buffer, (char)va_arg(args, int));
            break;
        case 's':
            append_string(buffer, va_arg(args, const char *));
            break;
        case 'd':
        case 'i':
            if (long_long_arg) {
                append_signed(buffer, va_arg(args, long long));
            } else if (long_arg) {
                append_signed(buffer, va_arg(args, long));
            } else {
                append_signed(buffer, va_arg(args, int));
            }
            break;
        case 'u':
            if (long_long_arg) {
                append_unsigned(buffer, va_arg(args, unsigned long long), 10u, false);
            } else if (long_arg) {
                append_unsigned(buffer, va_arg(args, unsigned long), 10u, false);
            } else {
                append_unsigned(buffer, va_arg(args, unsigned int), 10u, false);
            }
            break;
        case 'x':
        case 'X':
            if (long_long_arg) {
                append_unsigned(buffer, va_arg(args, unsigned long long), 16u, *fmt == 'X');
            } else if (long_arg) {
                append_unsigned(buffer, va_arg(args, unsigned long), 16u, *fmt == 'X');
            } else {
                append_unsigned(buffer, va_arg(args, unsigned int), 16u, *fmt == 'X');
            }
            break;
        case 'p':
            append_string(buffer, "0x");
            append_unsigned(buffer, (uintptr_t)va_arg(args, void *), 16u, false);
            break;
        default:
            append_char(buffer, '%');
            append_char(buffer, *fmt);
            break;
        }

        if (*fmt != '\0') {
            fmt++;
        }
    }
}

static void write_serial_line(const char *s) {
    serial_write_string(s);
}

static scdk_status_t write_console_line(const char *s, size_t len) {
    struct scdk_message msg;
    scdk_status_t status;

    if (log_console_endpoint == 0 || log_dispatching) {
        write_serial_line(s);
        return SCDK_OK;
    }

    status = scdk_message_init_write(&msg, 0, 0, s, len);
    if (status != SCDK_OK) {
        return status;
    }

    log_dispatching = true;
    status = scdk_endpoint_call(log_console_endpoint, &msg);
    log_dispatching = false;

    return status;
}

scdk_status_t scdk_log_set_console_endpoint(scdk_cap_t endpoint) {
    scdk_status_t status = scdk_cap_check(endpoint,
                                          SCDK_RIGHT_SEND,
                                          SCDK_OBJ_ENDPOINT,
                                          0);
    if (status != SCDK_OK) {
        return status;
    }

    log_console_endpoint = endpoint;
    return SCDK_OK;
}

void scdk_log_vwrite(const char *level, const char *fmt, va_list args) {
    struct scdk_log_buffer buffer = { 0 };

    if (level == 0) {
        level = "log";
    }

    if (fmt == 0) {
        fmt = "(null)";
    }

    append_string(&buffer, "[");
    append_string(&buffer, level);
    append_string(&buffer, "] ");
    append_format(&buffer, fmt, args);
    append_string(&buffer, "\n");

    if (write_console_line(buffer.data, buffer.len) != SCDK_OK) {
        write_serial_line(buffer.data);
    }
}

void scdk_log_write(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    scdk_log_vwrite(level, fmt, args);
    va_end(args);
}

void scdk_log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    scdk_log_vwrite("info", fmt, args);
    va_end(args);
}

void scdk_log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    scdk_log_vwrite("warn", fmt, args);
    va_end(args);
}

void scdk_log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    scdk_log_vwrite("error", fmt, args);
    va_end(args);
}
