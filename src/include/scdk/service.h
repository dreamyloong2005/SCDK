// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_SERVICE_H
#define SCDK_SERVICE_H

#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_MAX_SERVICES 32u

enum scdk_service_id {
    SCDK_SERVICE_NONE = 0,
    SCDK_SERVICE_CONSOLE = 1,
    SCDK_SERVICE_TMPFS = 2,
    SCDK_SERVICE_VFS = 3,
    SCDK_SERVICE_PROC = 4,
};

/*
 * Control-plane: bind a service ID to an endpoint capability.
 * Requires SCDK_RIGHT_BIND on an SCDK_OBJ_ENDPOINT capability.
 */
scdk_status_t scdk_service_register(uint64_t service_id,
                                    scdk_cap_t endpoint);

/*
 * Control-plane: resolve a service endpoint capability by service ID.
 * Requires no caller capability during bootstrap; future service lookup should
 * be mediated by namespace capabilities.
 */
scdk_status_t scdk_service_lookup(uint64_t service_id,
                                  scdk_cap_t *out_endpoint);

#endif
