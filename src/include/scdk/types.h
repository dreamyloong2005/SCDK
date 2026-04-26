// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_TYPES_H
#define SCDK_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef int64_t scdk_status_t;

#define SCDK_OK             0
#define SCDK_ERR_INVAL     -1
#define SCDK_ERR_NOMEM     -2
#define SCDK_ERR_NOENT     -3
#define SCDK_ERR_PERM      -4
#define SCDK_ERR_BOUNDS    -5
#define SCDK_ERR_BUSY      -6
#define SCDK_ERR_NOTSUP    -7

#define SCDK_BOOT_CORE 0u
#define SCDK_MAX_CORES 1u

#endif
