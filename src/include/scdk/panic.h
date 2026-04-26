// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_PANIC_H
#define SCDK_PANIC_H

__attribute__((noreturn)) void scdk_panic(const char *fmt, ...);

#define SCDK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            scdk_panic("assertion failed: %s:%u: %s", __FILE__, __LINE__, #expr); \
        } \
    } while (0)

#endif
