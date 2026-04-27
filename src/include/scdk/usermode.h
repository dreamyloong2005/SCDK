// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_USERMODE_H
#define SCDK_USERMODE_H

#include <stdint.h>

#include <scdk/types.h>

#define SCDK_USER_TEST_CODE_VIRT 0x0000000000400000ull
#define SCDK_USER_TEST_STACK_TOP 0x0000000070000000ull

/*
 * Control-plane: initialize x86_64 user-mode descriptors and syscall MSRs.
 * Requires a kernel stack top for syscall entry.
 */
scdk_status_t scdk_usermode_init(uint64_t syscall_stack_top);

/*
 * Control-plane diagnostic: create a built-in user address space, enter ring 3,
 * execute a debug syscall, and return through the exit syscall.
 */
scdk_status_t scdk_usermode_run_builtin_test(uint64_t hhdm_offset);

#endif
