#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 The Scadek OS Project contributors
set -u

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$ROOT/cross/bin:$ROOT/limine:$PATH"

missing=0

check_cmd() {
    local cmd="$1"
    local required="${2:-required}"

    if command -v "$cmd" >/dev/null 2>&1; then
        printf '[ok]      %-24s %s\n' "$cmd" "$(command -v "$cmd")"
    elif [ "$required" = "optional" ]; then
        printf '[optional] missing %-16s\n' "$cmd"
    else
        printf '[missing] %-24s\n' "$cmd"
        missing=1
    fi
}

check_cmd make
check_cmd xorriso
check_cmd mformat optional
check_cmd qemu-system-x86_64 optional
check_cmd nasm optional
check_cmd limine
check_cmd x86_64-elf-gcc
check_cmd x86_64-elf-ld
check_cmd x86_64-elf-objcopy

if command -v limine >/dev/null 2>&1; then
    limine --version | head -1
fi

if command -v x86_64-elf-gcc >/dev/null 2>&1; then
    x86_64-elf-gcc --version | head -1
fi

exit "$missing"
