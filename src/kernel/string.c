// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/string.h>

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}

void *memset(void *dest, int c, size_t n) {
    unsigned char *d = dest;

    for (size_t i = 0; i < n; i++) {
        d[i] = (unsigned char)c;
    }

    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d == s || n == 0) {
        return dest;
    }

    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

int memcmp(const void *lhs, const void *rhs, size_t n) {
    const unsigned char *l = lhs;
    const unsigned char *r = rhs;

    for (size_t i = 0; i < n; i++) {
        if (l[i] != r[i]) {
            return (int)l[i] - (int)r[i];
        }
    }

    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;

    while (s[len] != '\0') {
        len++;
    }

    return len;
}
