// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_SCHEDULER_H
#define SCDK_SCHEDULER_H

#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/object.h>
#include <scdk/types.h>

#define SCDK_MAX_TASKS 64u
#define SCDK_MAX_THREADS 128u
#define SCDK_KERNEL_STACK_SIZE 16384u

typedef void (*scdk_thread_entry_t)(void *arg);

enum scdk_thread_state {
    SCDK_THREAD_NONE = 0,
    SCDK_THREAD_NEW,
    SCDK_THREAD_READY,
    SCDK_THREAD_RUNNING,
    SCDK_THREAD_BLOCKED,
    SCDK_THREAD_DEAD,
    SCDK_THREAD_HALTED = SCDK_THREAD_DEAD,
};

/*
 * Control-plane: initialize the bootstrap scheduler model.
 * Creates a boot task and boot thread object, then marks them current.
 */
scdk_status_t scdk_scheduler_init(scdk_cap_t *out_boot_task,
                                  scdk_cap_t *out_boot_thread);

/*
 * Control-plane: create a task object skeleton.
 * Requires no capability during bootstrap; future callers should hold process
 * creation authority.
 */
scdk_status_t scdk_task_create(uint32_t owner_core,
                               scdk_object_id_t address_space,
                               scdk_cap_t *out_task);

/*
 * Control-plane: create a kernel thread object inside a task.
 * Requires SCDK_RIGHT_BIND on an SCDK_OBJ_TASK capability.
 */
scdk_status_t scdk_thread_create(scdk_cap_t task,
                                 scdk_thread_entry_t entry,
                                 void *arg,
                                 scdk_cap_t *out_thread);

/*
 * Control-plane: make a NEW kernel thread READY and enqueue it.
 * Requires SCDK_RIGHT_EXEC on an SCDK_OBJ_THREAD capability.
 */
scdk_status_t scdk_thread_start(scdk_cap_t thread);

/*
 * Scheduler data-plane: cooperatively yield the current CPU to another READY
 * thread on the same owner core.
 */
void scdk_yield(void);

/*
 * Scheduler control-plane test runner: drive READY threads until the run queue
 * is empty. No timer interrupt or preemption is involved.
 */
void scdk_scheduler_run(void);

/*
 * Control-plane diagnostic: read the current task capability.
 */
scdk_status_t scdk_scheduler_current_task(scdk_cap_t *out_task);

/*
 * Control-plane diagnostic: read the current thread capability.
 */
scdk_status_t scdk_scheduler_current_thread(scdk_cap_t *out_thread);

/*
 * Control-plane diagnostic: read a task's main thread capability.
 */
scdk_status_t scdk_task_main_thread(scdk_cap_t task,
                                    scdk_cap_t *out_thread);

/*
 * Control-plane diagnostic: read the task object ID owning a thread.
 */
scdk_status_t scdk_thread_task_id(scdk_cap_t thread,
                                  scdk_object_id_t *out_task_id);

/*
 * Control-plane diagnostic: read a thread state.
 */
scdk_status_t scdk_thread_state(scdk_cap_t thread,
                                uint32_t *out_state);

/*
 * Compatibility wrapper for early boot self-tests.
 * Performs one cooperative yield when the scheduler is initialized.
 */
scdk_status_t scdk_scheduler_yield_stub(void);

#endif
