#!/usr/bin/env sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 The Scadek OS Project contributors
set -eu

cd "$(dirname "$0")/.."
. .devtools/env.sh
make iso

exec qemu-system-x86_64 \
    -M q35 \
    -m 256M \
    -cdrom build/scdk.iso \
    -boot d \
    -serial stdio \
    -display none \
    -no-reboot
