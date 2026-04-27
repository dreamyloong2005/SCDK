// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_SYSCALL_H
#define SCDK_SYSCALL_H

#include <stdbool.h>
#include <stdint.h>

#include <scdk/types.h>

#define SCDK_SYS_DEBUG_WRITE 0ull
#define SCDK_SYS_EXIT        1ull

/*
 * Control-plane: initialize the minimal x86_64 syscall entry path.
 * Only debug-write and exit are supported in Milestone 15.
 */
scdk_status_t scdk_syscall_init(uint64_t kernel_stack_top);

/*
 * Architecture entry helper called from the syscall assembly path.
 * Returns 0 to resume user mode, or 1 to return to the saved kernel frame.
 */
uint64_t scdk_syscall_dispatch(uint64_t number,
                               uint64_t user_rsp,
                               uint64_t user_rip,
                               uint64_t user_rflags);

/*
 * Control-plane diagnostic: report whether the built-in user stub exited.
 */
bool scdk_syscall_user_exited(void);

#endif
