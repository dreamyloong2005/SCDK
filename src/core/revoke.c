// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/revoke.h>

scdk_status_t scdk_revoke_capability(scdk_cap_t cap) {
    return scdk_cap_revoke(cap);
}
