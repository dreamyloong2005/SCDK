/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (c) 2026 The Scadek OS Project contributors */

.set SCDK_SYS_EXIT, 3
.set SCDK_SYS_GRANT_CREATE, 4
.set SCDK_SYS_RING_CREATE, 6
.set SCDK_SYS_RING_BIND, 7
.set SCDK_SYS_RING_SUBMIT, 8
.set SCDK_SYS_RING_POLL, 9
.set SCDK_RING_OP_CONSOLE_WRITE, 1
.set SCDK_RIGHT_READ, 1
.set SCDK_RING_BATCH, 16
.set SCDK_RING_DESC_SIZE, 48
.set SCDK_RING_COMPLETION_SIZE, 32

.section .text
.global _start
.type _start, @function
_start:
    movq %rdi, %rbx

    movq $SCDK_SYS_GRANT_CREATE, %rax
    leaq ring_payload(%rip), %rdi
    movq $(ring_payload_end - ring_payload), %rsi
    movq $SCDK_RIGHT_READ, %rdx
    movq %rbx, %r10
    syscall
    movq %rax, %r12

    movq $SCDK_SYS_RING_CREATE, %rax
    movq $SCDK_RING_BATCH, %rdi
    syscall
    movq %rax, %r13

    movq $SCDK_SYS_RING_BIND, %rax
    movq %r13, %rdi
    movq %rbx, %rsi
    syscall
    testq %rax, %rax
    jne .Lbad

    subq $2048, %rsp
    movq %rsp, %r14
    movq $0, %r15

.Lfill_desc:
    cmpq $SCDK_RING_BATCH, %r15
    je .Lsubmit

    movq %r15, %rax
    imulq $SCDK_RING_DESC_SIZE, %rax, %rax
    leaq (%r14,%rax), %rsi

    movq $SCDK_RING_OP_CONSOLE_WRITE, 0(%rsi)
    movq %rbx, 8(%rsi)
    movq %r12, 16(%rsi)
    movq $0, 24(%rsi)
    movq $(ring_payload_end - ring_payload), 32(%rsi)
    movq %r15, 40(%rsi)

    incq %r15
    jmp .Lfill_desc

.Lsubmit:
    movq $SCDK_SYS_RING_SUBMIT, %rax
    movq %r13, %rdi
    movq %r14, %rsi
    movq $SCDK_RING_BATCH, %rdx
    syscall
    testq %rax, %rax
    jne .Lbad

    leaq 1024(%r14), %r15
    movq $SCDK_SYS_RING_POLL, %rax
    movq %r13, %rdi
    movq %r15, %rsi
    movq $SCDK_RING_BATCH, %rdx
    syscall
    cmpq $SCDK_RING_BATCH, %rax
    jne .Lbad

    movq $SCDK_SYS_EXIT, %rax
    syscall

.Lhalt:
    pause
    jmp .Lhalt

.Lbad:
    pause
    jmp .Lbad

ring_payload:
    .ascii "[user-ring] console write"
ring_payload_end:
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
