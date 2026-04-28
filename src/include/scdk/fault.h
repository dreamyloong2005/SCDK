// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_FAULT_H
#define SCDK_FAULT_H

#include <stdbool.h>
#include <stdint.h>

#include <scdk/types.h>

enum scdk_fault_vector {
    SCDK_FAULT_INVALID_OPCODE = 6u,
    SCDK_FAULT_GENERAL_PROTECTION = 13u,
    SCDK_FAULT_PAGE = 14u,
};

enum scdk_fault_user_test {
    SCDK_FAULT_TEST_PAGE_FAULT = 1u,
    SCDK_FAULT_TEST_INVALID_SYSCALL = 2u,
    SCDK_FAULT_TEST_BAD_POINTER = 3u,
};

struct scdk_fault_frame {
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

/*
 * Control-plane: install the minimal x86_64 exception gates used by M27.
 */
scdk_status_t scdk_fault_init(void);

/*
 * Architecture helper: install one extra x86_64 IDT gate after the M27 fault
 * table exists. Used by later interrupt sources such as the timer.
 */
scdk_status_t scdk_fault_install_gate(uint8_t vector, void (*handler)(void));

/*
 * Architecture entry point called by exception assembly stubs.
 * Returns 1 when a user fault was handled and control should return to the
 * saved kernel frame. Kernel faults do not return.
 */
uint64_t scdk_fault_dispatch(struct scdk_fault_frame *frame);

/*
 * Syscall fault paths for non-exception user errors.
 */
void scdk_fault_handle_invalid_syscall(uint64_t number);
void scdk_fault_handle_bad_user_pointer(uintptr_t user_ptr,
                                        scdk_status_t status);
void scdk_fault_handle_capability_failure(scdk_status_t status);

/*
 * Self-test helpers for M27 diagnostics.
 */
void scdk_fault_reset_test_state(void);
bool scdk_fault_saw_user_page_fault(void);
bool scdk_fault_saw_invalid_syscall(void);
bool scdk_fault_saw_bad_user_pointer(void);

#endif
