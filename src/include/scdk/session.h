// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_SESSION_H
#define SCDK_SESSION_H

#include <scdk/capability.h>
#include <scdk/types.h>

/*
 * M31 user-session namespace. A user program with this endpoint capability can
 * resolve the narrow set of boot services intended for ScadekOS startup.
 */
scdk_status_t scdk_session_service_init(scdk_cap_t *out_endpoint);

#endif
