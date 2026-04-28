#!/usr/bin/env sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 The Scadek OS Project contributors
set -eu

cd "$(dirname "$0")/.."

fail=0

report() {
    printf '%s\n' "$1"
    fail=1
}

matches() {
    rg -n "$1" src "$@" 2>/dev/null || true
}

hardware_violations="$(
matches 'scdk_serial_write_(char|string)|scdk_framebuffer_console_write|PS2_DATA_PORT|PS2_STATUS_PORT|inb\(|outb\(' |
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

if [ -n "$hardware_violations" ]; then
    printf '%s\n' "$hardware_violations"
    report "hardware access review failed"
fi

host_header_violations="$(matches '#include <(stdio|stdlib|unistd|sys/|pthread|fcntl|errno)')"
if [ -n "$host_header_violations" ]; then
    printf '%s\n' "$host_header_violations"
    report "host header review failed"
fi

posix_violations="$(matches '\b(fork|execve|waitpid|open|read|write|close)\s*\(')"
if [ -n "$posix_violations" ]; then
    printf '%s\n' "$posix_violations"
    report "native POSIX API review failed"
fi

if ! rg -q 'scdk_cap_check' src/core src/services src/ipc; then
    report "capability boundary review failed: no cap checks found"
fi

if ! rg -q 'scdk_user_validate_range|scdk_user_copy_from|scdk_user_copy_to' src/ipc src/core; then
    report "user pointer review failed: no checked user-copy path found"
fi

if ! rg -q 'scdk_ring_bound_target|scdk_ring_bind_target' src/core src/services; then
    report "ring review failed: no bound target checks found"
fi

if ! rg -q 'scdk_validate_grant_access|scdk_user_grant_copy_from' src/core src/services; then
    report "grant review failed: no grant validation path found"
fi

if [ "$fail" -ne 0 ]; then
    exit 1
fi

printf '%s\n' "architecture check pass"
