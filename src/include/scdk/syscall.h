// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_SYSCALL_H
#define SCDK_SYSCALL_H

#include <stdbool.h>
#include <stdint.h>

#include <scdk/syscall_numbers.h>
#include <scdk/types.h>

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

#endif
