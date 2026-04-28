// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_DEVMGR_H
#define SCDK_DEVMGR_H

#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_DEVMGR_MAX_DEVICES 8u
#define SCDK_DEVMGR_MAX_QUEUES 16u
#define SCDK_DEVMGR_FAKE_DEVICE_ID 1u

/*
 * Control-plane: start the kernel-resident device manager stub and register
 * its endpoint as SCDK_SERVICE_DEVMGR. Device and queue authorization remains
 * expressed through SCDK objects, capabilities, endpoint messages, and queue
 * capabilities.
 */
scdk_status_t scdk_devmgr_service_init(scdk_cap_t *out_endpoint);

#endif
