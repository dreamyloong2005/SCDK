// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_TMPFS_H
#define SCDK_TMPFS_H

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_TMPFS_MAX_OPEN_FILES 32u

/*
 * Control-plane: start the initrd-backed tmpfs service and register its
 * endpoint as SCDK_SERVICE_TMPFS.
 */
scdk_status_t scdk_tmpfs_service_init(scdk_cap_t *out_endpoint);

#endif
