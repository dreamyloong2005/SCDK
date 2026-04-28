/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (c) 2026 The Scadek OS Project contributors */

.set SCDK_SYS_ENDPOINT_CALL, 1
.set SCDK_SYS_EXIT, 3
.set SCDK_SERVICE_SESSION, 8
.set SCDK_SERVICE_PROC, 4
.set SCDK_MSG_SERVICE_LOOKUP, 8
.set SCDK_MSG_PROCESS_SPAWN, 9

.section .text
.global _start
.type _start, @function
_start:
    movq %rdi, %rbx

    subq $64, %rsp
    movq %rsp, %rsi

    movq $0, 0(%rsi)
    movq $SCDK_SERVICE_SESSION, 8(%rsi)
    movq $SCDK_MSG_SERVICE_LOOKUP, 16(%rsi)
    movq $SCDK_SERVICE_PROC, 24(%rsi)
    movq $0, 32(%rsi)
    movq $0, 40(%rsi)
    movq $0, 48(%rsi)
    movq $0, 56(%rsi)

    movq $SCDK_SYS_ENDPOINT_CALL, %rax
    movq %rbx, %rdi
    syscall

    movq 24(%rsi), %r12

    movq $0, 0(%rsi)
    movq $SCDK_SERVICE_PROC, 8(%rsi)
    movq $SCDK_MSG_PROCESS_SPAWN, 16(%rsi)
    leaq hello_path(%rip), %rax
    movq %rax, 24(%rsi)
    movq $(hello_path_end - hello_path), 32(%rsi)
    movq $0, 40(%rsi)
    movq $0, 48(%rsi)
    movq $0, 56(%rsi)

    movq $SCDK_SYS_ENDPOINT_CALL, %rax
    movq %r12, %rdi
    syscall

    movq $SCDK_SYS_EXIT, %rax
    syscall

.Lhalt:
    hlt
    jmp .Lhalt

hello_path:
    .ascii "/hello"
hello_path_end:
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
