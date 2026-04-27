// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_USER_RING_H
#define SCDK_USER_RING_H

#include <stdint.h>

#include <scdk/ring.h>
#include <scdk/types.h>

#define SCDK_MAX_USER_RINGS 64u

/*
 * Control-plane syscall helper: create a user-visible ring owned by the
 * current user task. The returned cap is still an SCDK_OBJ_RING capability.
 */
scdk_status_t scdk_user_ring_create(scdk_cap_t source_task,
                                    uint32_t entries,
                                    scdk_cap_t *out_ring);

/*
 * Control-plane syscall helper: bind a user ring to a service endpoint.
 * Requires the ring to belong to source_task and target_endpoint to be SEND.
 */
scdk_status_t scdk_user_ring_bind(scdk_cap_t source_task,
                                  scdk_cap_t ring_cap,
                                  scdk_cap_t target_endpoint);

/*
 * Data-plane syscall helper: copy descriptors from user memory, submit them
 * to the ring, and synchronously kick the bound endpoint in this prototype.
 */
scdk_status_t scdk_user_ring_submit(scdk_cap_t source_task,
                                    scdk_cap_t ring_cap,
                                    uintptr_t user_descs,
                                    uint32_t count);

/*
 * Data-plane syscall helper: poll completions into user memory.
 */
scdk_status_t scdk_user_ring_poll(scdk_cap_t source_task,
                                  scdk_cap_t ring_cap,
                                  uintptr_t user_completions,
                                  uint32_t max_count,
                                  uint32_t *out_count);

#endif
