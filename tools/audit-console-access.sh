#!/usr/bin/env sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 The Scadek OS Project contributors
set -eu

cd "$(dirname "$0")/.."

violations="$(
rg -n 'scdk_serial_write_(char|string)|scdk_framebuffer_console_write|PS2_DATA_PORT|PS2_STATUS_PORT|inb\(|outb\(' src |
while IFS= read -r line; do
    case "$line" in
        src/kernel/early_console.c:*|\
        src/kernel/panic.c:*|\
        src/drivers/console_backend.c:*|\
        src/drivers/input/ps2_keyboard.c:*|\
        src/arch/x86_64/serial.c:*|\
        src/arch/x86_64/framebuffer.c:*|\
        src/arch/x86_64/timer.c:*|\
        src/include/scdk/serial.h:*|\
        src/include/scdk/framebuffer.h:*)
            ;;
        *)
            printf '%s\n' "$line"
            ;;
    esac
done
)"

if [ -n "$violations" ]; then
    printf '%s\n' "$violations"
    exit 1
fi

printf '%s\n' "console/input hardware access audit pass"
