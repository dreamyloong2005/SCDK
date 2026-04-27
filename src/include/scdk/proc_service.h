// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_PROC_SERVICE_H
#define SCDK_PROC_SERVICE_H

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_PROC_MAX_PATH 128u

/*
 * Control-plane: start the kernel-resident process manager stub and register
 * its endpoint as SCDK_SERVICE_PROC. Spawn requests are still SCDK-native
 * endpoint/message operations and route through VFS plus the loader.
 */
scdk_status_t scdk_proc_service_init(uint64_t hhdm_offset,
                                     scdk_cap_t *out_endpoint);

#endif
