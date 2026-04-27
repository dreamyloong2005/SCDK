// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_USER_GRANT_H
#define SCDK_USER_GRANT_H

#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_MAX_USER_GRANTS 128u

/*
 * Control-plane syscall helper: create a user-visible grant from the current
 * task's user address range to a target endpoint.
 */
scdk_status_t scdk_user_grant_create(scdk_cap_t source_task,
                                     uintptr_t user_base,
                                     uint64_t length,
                                     uint64_t rights,
                                     scdk_cap_t target_endpoint,
                                     scdk_cap_t *out_grant);

/*
 * Control-plane: revoke a user-visible grant. Future access checks fail.
 */
scdk_status_t scdk_user_grant_revoke(scdk_cap_t grant_cap);

/*
 * Data-plane: verify a user-visible grant's revocation state, rights, and
 * bounds for the requested range.
 */
scdk_status_t scdk_validate_grant_access(scdk_cap_t grant_cap,
                                         uint64_t offset,
                                         uint64_t length,
                                         uint64_t required_rights);

/*
 * Data-plane prototype: copy bytes from a user grant for a bound target
 * endpoint. This keeps services from reading raw user pointers directly.
 */
scdk_status_t scdk_user_grant_copy_from(scdk_cap_t target_endpoint,
                                        scdk_cap_t grant_cap,
                                        uint64_t offset,
                                        void *kernel_dst,
                                        uint64_t length);

#endif
