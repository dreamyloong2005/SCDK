/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (c) 2026 The Scadek OS Project contributors */

.set SCDK_SYS_ENDPOINT_CALL, 1
.set SCDK_SYS_EXIT, 3
.set SCDK_SERVICE_CONSOLE, 1
.set SCDK_MSG_WRITE, 4

.section .text
.global _start
.type _start, @function
_start:
    movq %rdi, %rbx

    subq $64, %rsp
    movq %rsp, %rsi

    movq $0, 0(%rsi)
    movq $SCDK_SERVICE_CONSOLE, 8(%rsi)
    movq $SCDK_MSG_WRITE, 16(%rsi)
    leaq hello_message(%rip), %rax
    movq %rax, 24(%rsi)
    movq $(hello_message_end - hello_message), 32(%rsi)
    movq $0, 40(%rsi)
    movq $0, 48(%rsi)
    movq $0, 56(%rsi)

    movq $SCDK_SYS_ENDPOINT_CALL, %rax
    movq %rbx, %rdi
    syscall

    movq $SCDK_SYS_EXIT, %rax
    syscall

.Lhalt:
    hlt
    jmp .Lhalt

hello_message:
    .ascii "[user] hello from spawned process"
hello_message_end:
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
