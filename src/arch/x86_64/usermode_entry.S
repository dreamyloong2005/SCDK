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

    movq %rdi, %r8
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
    pushq %r8
    movq %rdx, %rdi
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
    movq %rdi, scdk_syscall_arg0(%rip)
    movq %rsi, scdk_syscall_arg1(%rip)
    movq %rdx, scdk_syscall_arg2(%rip)
    movq %r10, scdk_syscall_arg3(%rip)

    movq scdk_syscall_kernel_stack_top(%rip), %rsp
    movq %rax, %rdi
    movq scdk_syscall_arg0(%rip), %rsi
    movq scdk_syscall_arg1(%rip), %rdx
    movq scdk_syscall_arg2(%rip), %rcx
    movq scdk_syscall_arg3(%rip), %r8
    movq scdk_syscall_user_rsp(%rip), %r9
    call scdk_syscall_dispatch

    cmpq $1, %rax
    je .Lsyscall_exit

    movq scdk_syscall_return_value(%rip), %rax
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
    movq %rdi, %rbx

    movq $0, %rax
    syscall

    subq $64, %rsp
    movq %rsp, %rsi

    movq $0, 0(%rsi)
    movq $1, 8(%rsi)
    movq $4, 16(%rsi)
    leaq .Luser_console_write(%rip), %rax
    movq %rax, 24(%rsi)
    movq $(.Luser_console_write_end - .Luser_console_write), 32(%rsi)
    movq $0, 40(%rsi)
    movq $0, 48(%rsi)
    movq $0, 56(%rsi)

    movq $1, %rax
    movq %rbx, %rdi
    syscall

    movq $3, %rax
    syscall
.Luser_halt:
    hlt
    jmp .Luser_halt
.Luser_console_write:
    .ascii "[user] endpoint call console write pass"
.Luser_console_write_end:
scdk_user_test_stub_end:
.size scdk_user_test_stub_start, . - scdk_user_test_stub_start

.section .note.GNU-stack,"",@progbits
