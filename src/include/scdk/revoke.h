// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_REVOKE_H
#define SCDK_REVOKE_H

#include <scdk/capability.h>
#include <scdk/types.h>

/*
 * Control-plane: revoke a capability through the M26 revocation interface.
 * The token becomes stale immediately and dependent bindings must reject it on
 * future use.
 */
scdk_status_t scdk_revoke_capability(scdk_cap_t cap);

#endif
