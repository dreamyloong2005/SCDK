// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_SYSCALL_H
#define SCDK_SYSCALL_H

#include <stdbool.h>
#include <stdint.h>

#include <scdk/syscall_numbers.h>
#include <scdk/types.h>

struct scdk_syscall_task_state {
    uint64_t kernel_stack_top;
    uint64_t user_rsp;
    uint64_t user_rip;
    uint64_t user_rflags;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t return_value;
    uint64_t user_return_rsp;
    uint64_t user_return_rip;
    bool user_exited;
    bool endpoint_call_passed;
    bool user_faulted;
    scdk_status_t user_fault_status;
};

/*
 * Control-plane: initialize the minimal x86_64 syscall entry path.
 * Milestone 16 supports debug-write, endpoint-call, yield, and exit.
 */
scdk_status_t scdk_syscall_init(uint64_t kernel_stack_top);

/*
 * Architecture diagnostic: report whether syscall MSRs are initialized.
 */
bool scdk_syscall_ready(void);

/*
 * Control-plane diagnostic: reset per-user-stub syscall observations.
 */
void scdk_syscall_reset_task_state(void);

/*
 * Control-plane: preserve the current minimal syscall/user return state while
 * a kernel service runs another user task from inside a syscall path.
 */
void scdk_syscall_save_task_state(struct scdk_syscall_task_state *out_state);

/*
 * Control-plane: restore syscall/user return state saved before a nested user
 * task run.
 */
void scdk_syscall_restore_task_state(const struct scdk_syscall_task_state *state);

/*
 * Architecture entry helper called from the syscall assembly path.
 * Returns 0 to resume user mode, or 1 to return to the saved kernel frame.
 */
uint64_t scdk_syscall_dispatch(uint64_t number,
                               uint64_t arg0,
                               uint64_t arg1,
                               uint64_t arg2,
                               uint64_t arg3,
                               uint64_t user_rsp);

/*
 * Control-plane diagnostic: report whether the built-in user stub exited.
 */
bool scdk_syscall_user_exited(void);

/*
 * Control-plane diagnostic: report whether the user stub completed an
 * endpoint-call syscall successfully.
 */
bool scdk_syscall_endpoint_call_passed(void);

/*
 * Fault-path helper: mark the current user-mode run as faulted so the
 * assembly return path unwinds to the saved kernel frame.
 */
void scdk_syscall_mark_user_fault(scdk_status_t status);

/*
 * Fault-path diagnostic: report whether the current user-mode run faulted.
 */
bool scdk_syscall_user_faulted(void);

/*
 * Fault-path diagnostic: status associated with the last user fault.
 */
scdk_status_t scdk_syscall_user_fault_status(void);

#endif
