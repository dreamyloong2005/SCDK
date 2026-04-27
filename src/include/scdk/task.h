// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_TASK_H
#define SCDK_TASK_H

#include <stddef.h>

#include <scdk/scheduler.h>

typedef scdk_object_id_t scdk_task_id_t;

enum scdk_task_state {
    SCDK_TASK_NONE = 0,
    SCDK_TASK_NEW,
    SCDK_TASK_RUNNING,
    SCDK_TASK_DEAD,
    SCDK_TASK_CLEANED,
};

/*
 * Control-plane: create a minimal user task with its own address space and
 * main user thread object. The task remains SCDK-native and capability-aware.
 */
scdk_status_t scdk_user_task_create(scdk_cap_t *out_task,
                                    scdk_cap_t *out_address_space,
                                    scdk_cap_t *out_main_thread);

/*
 * Control-plane: create a user thread metadata entry inside a task.
 * Requires SCDK_RIGHT_BIND on an SCDK_OBJ_TASK capability.
 */
scdk_status_t scdk_user_thread_create(scdk_cap_t task,
                                      scdk_cap_t *out_thread);

/*
 * Control-plane test path: run the task's built-in user stub, then return to
 * kernel mode through SCDK_SYS_EXIT.
 */
scdk_status_t scdk_user_task_run_builtin(scdk_cap_t task,
                                         uint64_t hhdm_offset);

/*
 * Control-plane loader path: run a flat executable image in a NEW user task's
 * address space and return after the task exits through SCDK_SYS_EXIT.
 */
scdk_status_t scdk_user_task_run_flat(scdk_cap_t task,
                                      const void *image,
                                      size_t image_size,
                                      scdk_cap_t bootstrap_endpoint,
                                      uint64_t hhdm_offset);

/*
 * Control-plane: mark a user task dead after the user exit path returns.
 */
scdk_status_t scdk_task_exit(scdk_cap_t task,
                             int status_code);

/*
 * Control-plane: run the current v1 cleanup path for a dead user task.
 */
scdk_status_t scdk_task_cleanup(scdk_cap_t task);

/*
 * Control-plane diagnostic: read lifecycle state for a user task.
 */
scdk_status_t scdk_user_task_state(scdk_cap_t task,
                                   uint32_t *out_state);

/*
 * Control-plane diagnostic: read the user task currently executing on this CPU
 * during a prototype ring 3 entry.
 */
scdk_status_t scdk_user_task_current(scdk_cap_t *out_task);

#endif
