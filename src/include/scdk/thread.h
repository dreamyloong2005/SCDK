// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_THREAD_H
#define SCDK_THREAD_H

#include <scdk/scheduler.h>

typedef scdk_object_id_t scdk_thread_id_t;

/*
 * Control-plane diagnostic: read lifecycle state for a user thread.
 */
scdk_status_t scdk_user_thread_state(scdk_cap_t thread,
                                     uint32_t *out_state);

#endif
