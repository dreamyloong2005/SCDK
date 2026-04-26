#!/usr/bin/env sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 The Scadek OS Project contributors

# Source this file before building SCDK from a fresh shell:
#   . .devtools/env.sh

if [ -d ".devtools/limine" ]; then
    SCDK_DEVTOOLS="$(pwd)/.devtools"
else
    SCDK_DEVTOOLS=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
fi

export SCDK_DEVTOOLS
export PATH="$SCDK_DEVTOOLS/cross/bin:$SCDK_DEVTOOLS/limine:$PATH"
