// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/panic.h>

#include <stdarg.h>

#include <scdk/log.h>

static void dump_registers_placeholder(void) {
    scdk_log_write("panic", "register dump placeholder:");
    scdk_log_write("panic", "  rip/rsp/rflags unavailable before IDT/ISR setup");
    scdk_log_write("panic", "  fault vector unavailable before exception handlers");
}

__attribute__((noreturn)) static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

__attribute__((noreturn)) void scdk_panic(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    scdk_log_vwrite("panic", fmt, args);
    va_end(args);

    dump_registers_placeholder();
    scdk_log_write("panic", "halting");
    halt_forever();
}
