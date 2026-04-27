/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (c) 2026 The Scadek OS Project contributors */

.set SCDK_SYS_ENDPOINT_CALL, 1
.set SCDK_SYS_EXIT, 3
.set SCDK_SYS_GRANT_CREATE, 4
.set SCDK_SYS_GRANT_REVOKE, 5
.set SCDK_SERVICE_GRANT_TEST, 5
.set SCDK_MSG_READ, 3
.set SCDK_RIGHT_READ, 1

.section .text
.global _start
.type _start, @function
_start:
    movq %rdi, %rbx

    movq $SCDK_SYS_GRANT_CREATE, %rax
    leaq grant_payload(%rip), %rdi
    movq $(grant_payload_end - grant_payload), %rsi
    movq $SCDK_RIGHT_READ, %rdx
    movq %rbx, %r10
    syscall
    movq %rax, %r12

    subq $64, %rsp
    movq %rsp, %rsi

    movq $0, 0(%rsi)
    movq $SCDK_SERVICE_GRANT_TEST, 8(%rsi)
    movq $SCDK_MSG_READ, 16(%rsi)
    movq %r12, 24(%rsi)
    movq $0, 32(%rsi)
    movq $(grant_payload_end - grant_payload), 40(%rsi)
    movq $1, 48(%rsi)
    movq $0, 56(%rsi)

    movq $SCDK_SYS_ENDPOINT_CALL, %rax
    movq %rbx, %rdi
    syscall

    movq $SCDK_SYS_GRANT_REVOKE, %rax
    movq %r12, %rdi
    syscall

    movq %rsp, %rsi
    movq $0, 0(%rsi)
    movq $SCDK_SERVICE_GRANT_TEST, 8(%rsi)
    movq $SCDK_MSG_READ, 16(%rsi)
    movq %r12, 24(%rsi)
    movq $0, 32(%rsi)
    movq $(grant_payload_end - grant_payload), 40(%rsi)
    movq $2, 48(%rsi)
    movq $0, 56(%rsi)

    movq $SCDK_SYS_ENDPOINT_CALL, %rax
    movq %rbx, %rdi
    syscall

    movq $SCDK_SYS_EXIT, %rax
    syscall

.Lhalt:
    hlt
    jmp .Lhalt

grant_payload:
    .ascii "grant payload"
grant_payload_end:
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
