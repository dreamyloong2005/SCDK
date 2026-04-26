// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_CONSOLE_SERVICE_H
#define SCDK_CONSOLE_SERVICE_H

#include <scdk/capability.h>
#include <scdk/types.h>

/*
 * Control-plane: create and register the kernel-resident console service.
 * The returned endpoint accepts SCDK_MSG_WRITE messages.
 */
scdk_status_t scdk_console_service_init(scdk_cap_t *out_endpoint);

#endif
