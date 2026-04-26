// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_GRANT_H
#define SCDK_GRANT_H

#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_MAX_GRANTS 128u

struct scdk_grant {
    scdk_cap_t frame_cap;
    uintptr_t base;
    uint64_t offset;
    uint64_t length;
    uint64_t rights;
    uint32_t lifetime;
    uint32_t refcount;
};

/*
 * Control-plane: create a grant over an existing bootstrap memory range.
 * The returned cap carries the same READ/WRITE/MAP rights stored in the grant.
 */
scdk_status_t scdk_grant_create(void *base,
                                uint64_t length,
                                uint64_t rights,
                                uint32_t lifetime,
                                scdk_cap_t *out_grant);

/*
 * Data-plane: verify rights and bounds for a grant range.
 * Requires required_rights on an SCDK_OBJ_GRANT capability.
 */
scdk_status_t scdk_grant_check(scdk_cap_t grant,
                               uint64_t required_rights,
                               uint64_t offset,
                               uint64_t length);

/*
 * Data-plane prototype helper: resolve a checked grant range to a bootstrap
 * kernel pointer. Future VMM work should replace this with mapping semantics.
 */
scdk_status_t scdk_grant_resolve(scdk_cap_t grant,
                                 uint64_t required_rights,
                                 uint64_t offset,
                                 uint64_t length,
                                 void **out_ptr);

#endif
