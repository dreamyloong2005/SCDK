// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_CAPABILITY_H
#define SCDK_CAPABILITY_H

#include <stdint.h>

#include <scdk/object.h>
#include <scdk/types.h>

#define SCDK_MAX_CAPS 4096u

typedef uint64_t scdk_cap_t;

enum scdk_rights {
    SCDK_RIGHT_READ   = 1ull << 0,
    SCDK_RIGHT_WRITE  = 1ull << 1,
    SCDK_RIGHT_EXEC   = 1ull << 2,
    SCDK_RIGHT_MAP    = 1ull << 3,
    SCDK_RIGHT_SEND   = 1ull << 4,
    SCDK_RIGHT_RECV   = 1ull << 5,
    SCDK_RIGHT_BIND   = 1ull << 6,
    SCDK_RIGHT_REVOKE = 1ull << 7,
};

struct scdk_cap_entry {
    scdk_object_id_t object_id;
    uint32_t generation;
    uint32_t object_type;
    uint64_t rights;
};

/*
 * Control-plane: create a capability for an existing object.
 * Requires no capability during bootstrap; future callers should hold authority
 * to mint or delegate the requested rights.
 */
scdk_status_t scdk_cap_create(scdk_object_id_t object_id,
                              uint64_t rights,
                              scdk_cap_t *out_cap);

/*
 * Control-plane: look up a capability table entry.
 * Requires the cap token itself; also rejects stale object generations.
 */
scdk_status_t scdk_cap_lookup(scdk_cap_t cap,
                              const struct scdk_cap_entry **out_entry);

/*
 * Control-plane: verify that a cap grants rights to an object type.
 * Requires expected_type to match unless expected_type is SCDK_OBJ_NONE.
 */
scdk_status_t scdk_cap_check(scdk_cap_t cap,
                             uint64_t required_rights,
                             uint32_t expected_type,
                             const struct scdk_cap_entry **out_entry);

/*
 * Control-plane: revoke placeholder.
 * Requires a valid cap token; full revocation waits for generation-aware users.
 */
scdk_status_t scdk_cap_revoke_stub(scdk_cap_t cap);

#endif
