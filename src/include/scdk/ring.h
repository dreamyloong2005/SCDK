// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_RING_H
#define SCDK_RING_H

#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_MAX_RINGS 64u
#define SCDK_RING_MAX_ENTRIES 64u

struct scdk_ring_desc {
    uint64_t op;
    uint64_t cap;
    uint64_t grant;
    uint64_t offset;
    uint64_t length;
    uint64_t flags;
};

struct scdk_completion {
    uint64_t op;
    uint64_t status;
    uint64_t result0;
    uint64_t result1;
};

struct scdk_ring {
    uint32_t entries;
    uint32_t submit_head;
    uint32_t submit_tail;
    uint32_t complete_head;
    uint32_t complete_tail;

    struct scdk_ring_desc *sq;
    struct scdk_completion *cq;

    scdk_cap_t bound_target;
};

/*
 * Control-plane: create a static ring object and mint its bootstrap cap.
 * The returned cap carries READ/WRITE for queue access and BIND for later
 * target binding policy.
 */
scdk_status_t scdk_ring_create(uint32_t owner_core,
                               uint32_t entries,
                               scdk_cap_t bound_target,
                               scdk_cap_t *out_ring);

/*
 * Data-plane: submit one operation descriptor.
 * Requires SCDK_RIGHT_WRITE on an SCDK_OBJ_RING capability.
 */
scdk_status_t scdk_ring_submit(scdk_cap_t ring,
                               const struct scdk_ring_desc *desc);

/*
 * Data-plane: consume one submitted descriptor.
 * Requires SCDK_RIGHT_READ on an SCDK_OBJ_RING capability.
 */
scdk_status_t scdk_ring_consume(scdk_cap_t ring,
                                struct scdk_ring_desc *out_desc);

/*
 * Data-plane: publish one completion entry.
 * Requires SCDK_RIGHT_WRITE on an SCDK_OBJ_RING capability.
 */
scdk_status_t scdk_ring_complete(scdk_cap_t ring,
                                 const struct scdk_completion *completion);

/*
 * Data-plane: poll one completion entry.
 * Requires SCDK_RIGHT_READ on an SCDK_OBJ_RING capability.
 */
scdk_status_t scdk_ring_poll(scdk_cap_t ring,
                             struct scdk_completion *out_completion);

#endif
