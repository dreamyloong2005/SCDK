// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/syscall.h>

#include <scdk/log.h>

#define MSR_EFER  0xc0000080u
#define MSR_STAR  0xc0000081u
#define MSR_LSTAR 0xc0000082u
#define MSR_FMASK 0xc0000084u

#define EFER_SCE (1ull << 0)

#define X86_KERNEL_CODE_SEL 0x08ull
#define X86_SYSRET_USER_BASE_SEL 0x18ull

#define RFLAGS_TF (1ull << 8)
#define RFLAGS_IF (1ull << 9)
#define RFLAGS_DF (1ull << 10)
#define RFLAGS_AC (1ull << 18)

extern void scdk_syscall_entry(void);

uint64_t scdk_syscall_kernel_stack_top;
uint64_t scdk_syscall_user_rsp;
uint64_t scdk_syscall_user_rip;
uint64_t scdk_syscall_user_rflags;
uint64_t scdk_syscall_arg0;
uint64_t scdk_syscall_arg1;
uint64_t scdk_syscall_arg2;
uint64_t scdk_syscall_arg3;
uint64_t scdk_user_return_rsp;
uint64_t scdk_user_return_rip;

static bool syscall_initialized;

static uint64_t rdmsr(uint32_t msr) {
    uint32_t lo;
    uint32_t hi;

    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32u) | lo;
}

static void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32u);

    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi) : "memory");
}

scdk_status_t scdk_syscall_init(uint64_t kernel_stack_top) {
    uint64_t star;
    uint64_t efer;

    if (kernel_stack_top == 0u || (kernel_stack_top & 0xfull) != 0u) {
        return SCDK_ERR_INVAL;
    }

    scdk_syscall_kernel_stack_top = kernel_stack_top;
    scdk_syscall_reset_task_state();

    star = (X86_SYSRET_USER_BASE_SEL << 48u) | (X86_KERNEL_CODE_SEL << 32u);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)&scdk_syscall_entry);
    wrmsr(MSR_FMASK, RFLAGS_TF | RFLAGS_IF | RFLAGS_DF | RFLAGS_AC);

    efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | EFER_SCE);

    syscall_initialized = true;
    scdk_log_write("syscall", "dispatch ready");
    return SCDK_OK;
}

bool scdk_syscall_ready(void) {
    return syscall_initialized;
}
