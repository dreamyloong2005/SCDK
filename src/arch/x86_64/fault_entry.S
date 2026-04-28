/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (c) 2026 The Scadek OS Project contributors */

.text

.global scdk_isr_invalid_opcode
.type scdk_isr_invalid_opcode, @function
scdk_isr_invalid_opcode:
    pushq $0
    pushq $6
    jmp scdk_isr_common
.size scdk_isr_invalid_opcode, . - scdk_isr_invalid_opcode

.global scdk_isr_general_protection
.type scdk_isr_general_protection, @function
scdk_isr_general_protection:
    pushq $13
    jmp scdk_isr_common
.size scdk_isr_general_protection, . - scdk_isr_general_protection

.global scdk_isr_page_fault
.type scdk_isr_page_fault, @function
scdk_isr_page_fault:
    pushq $14
    jmp scdk_isr_common
.size scdk_isr_page_fault, . - scdk_isr_page_fault

.type scdk_isr_common, @function
scdk_isr_common:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rdi
    pushq %rsi
    pushq %rbp
    pushq %rbx
    pushq %rdx
    pushq %rcx
    pushq %rax

    leaq 120(%rsp), %rdi
    call scdk_fault_dispatch

    cmpq $1, %rax
    je .Lfault_return_to_kernel

    popq %rax
    popq %rcx
    popq %rdx
    popq %rbx
    popq %rbp
    popq %rsi
    popq %rdi
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    addq $16, %rsp
    iretq

.Lfault_return_to_kernel:
    movq scdk_user_return_rsp(%rip), %rsp
    jmp *scdk_user_return_rip(%rip)
.size scdk_isr_common, . - scdk_isr_common

.section .note.GNU-stack,"",@progbits
