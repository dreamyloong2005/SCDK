/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (c) 2026 The Scadek OS Project contributors */

.set X86_USER_DATA_SEL, 0x23
.set X86_USER_CODE_SEL, 0x2b

.text
.global scdk_enter_usermode_asm
.type scdk_enter_usermode_asm, @function
scdk_enter_usermode_asm:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    pushq %rbp

    movq %rsp, scdk_user_return_rsp(%rip)
    leaq .Luser_return(%rip), %rax
    movq %rax, scdk_user_return_rip(%rip)

    movw $X86_USER_DATA_SEL, %ax
    movw %ax, %ds
    movw %ax, %es

    pushq $X86_USER_DATA_SEL
    pushq %rsi
    pushq $0x2
    pushq $X86_USER_CODE_SEL
    pushq %rdi
    iretq

.Luser_return:
    popq %rbp
    popq %rbx
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    ret
.size scdk_enter_usermode_asm, . - scdk_enter_usermode_asm

.global scdk_syscall_entry
.type scdk_syscall_entry, @function
scdk_syscall_entry:
    movq %rsp, scdk_syscall_user_rsp(%rip)
    movq %rcx, scdk_syscall_user_rip(%rip)
    movq %r11, scdk_syscall_user_rflags(%rip)

    movq scdk_syscall_kernel_stack_top(%rip), %rsp
    movq %rax, %rdi
    movq scdk_syscall_user_rsp(%rip), %rsi
    movq scdk_syscall_user_rip(%rip), %rdx
    movq scdk_syscall_user_rflags(%rip), %rcx
    call scdk_syscall_dispatch

    cmpq $1, %rax
    je .Lsyscall_exit

    movq scdk_syscall_user_rip(%rip), %rcx
    movq scdk_syscall_user_rflags(%rip), %r11
    movq scdk_syscall_user_rsp(%rip), %rsp
    sysretq

.Lsyscall_exit:
    movq scdk_user_return_rsp(%rip), %rsp
    jmp *scdk_user_return_rip(%rip)
.size scdk_syscall_entry, . - scdk_syscall_entry

.global scdk_user_test_stub_start
.global scdk_user_test_stub_end
.type scdk_user_test_stub_start, @function
scdk_user_test_stub_start:
    movq $0, %rax
    syscall
    movq $1, %rax
    syscall
.Luser_halt:
    hlt
    jmp .Luser_halt
scdk_user_test_stub_end:
.size scdk_user_test_stub_start, . - scdk_user_test_stub_start

.section .note.GNU-stack,"",@progbits
