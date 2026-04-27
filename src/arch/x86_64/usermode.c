// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/usermode.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/address_space.h>
#include <scdk/capability.h>
#include <scdk/log.h>
#include <scdk/mm.h>
#include <scdk/object.h>
#include <scdk/service.h>
#include <scdk/string.h>
#include <scdk/syscall.h>

#define X86_KERNEL_CODE_SEL 0x08u
#define X86_KERNEL_DATA_SEL 0x10u
#define X86_SYSRET_BASE_SEL 0x18u
#define X86_USER_DATA_SEL   0x20u
#define X86_USER_CODE_SEL   0x28u
#define X86_TSS_SEL         0x30u

#define GDT_ACCESS_KERNEL_CODE 0x9au
#define GDT_ACCESS_KERNEL_DATA 0x92u
#define GDT_ACCESS_USER_CODE   0xfau
#define GDT_ACCESS_USER_DATA   0xf2u
#define GDT_ACCESS_TSS         0x89u

#define GDT_FLAGS_LONG_CODE 0xau
#define GDT_FLAGS_DATA      0xcu

struct gdt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct x86_tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

extern void scdk_enter_usermode_asm(uint64_t entry,
                                    uint64_t stack_top,
                                    scdk_cap_t bootstrap_endpoint);
extern const uint8_t scdk_user_test_stub_start[];
extern const uint8_t scdk_user_test_stub_end[];

static uint64_t gdt[8];
static struct x86_tss tss;
static bool usermode_initialized;

static uint8_t syscall_stack[SCDK_PAGE_SIZE] __attribute__((aligned(16)));

static uint64_t segment_descriptor(uint8_t access, uint8_t flags) {
    uint64_t limit = 0xfffffull;

    return (limit & 0xffffu) |
           ((limit >> 16u) << 48u) |
           ((uint64_t)access << 40u) |
           ((uint64_t)flags << 52u);
}

static void set_tss_descriptor(uint32_t index, uintptr_t base, uint32_t limit) {
    uint64_t low = 0;
    uint64_t high = 0;

    low |= (uint64_t)(limit & 0xffffu);
    low |= (uint64_t)(base & 0xffffffu) << 16u;
    low |= (uint64_t)GDT_ACCESS_TSS << 40u;
    low |= (uint64_t)((limit >> 16u) & 0x0fu) << 48u;
    low |= (uint64_t)((base >> 24u) & 0xffu) << 56u;
    high = (uint64_t)(base >> 32u);

    gdt[index] = low;
    gdt[index + 1u] = high;
}

static void load_gdt_and_tss(void) {
    struct gdt_pointer pointer = {
        .limit = sizeof(gdt) - 1u,
        .base = (uint64_t)(uintptr_t)gdt
    };

    __asm__ volatile (
        "lgdt %0\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "movw $0x30, %%ax\n"
        "ltr %%ax\n"
        :
        : "m"(pointer)
        : "rax", "memory");
}

scdk_status_t scdk_usermode_init(uint64_t syscall_stack_top) {
    if (syscall_stack_top == 0u || (syscall_stack_top & 0xfull) != 0u) {
        return SCDK_ERR_INVAL;
    }

    memset(gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    gdt[1] = segment_descriptor(GDT_ACCESS_KERNEL_CODE, GDT_FLAGS_LONG_CODE);
    gdt[2] = segment_descriptor(GDT_ACCESS_KERNEL_DATA, GDT_FLAGS_DATA);
    gdt[3] = segment_descriptor(GDT_ACCESS_USER_DATA, GDT_FLAGS_DATA);
    gdt[4] = segment_descriptor(GDT_ACCESS_USER_DATA, GDT_FLAGS_DATA);
    gdt[5] = segment_descriptor(GDT_ACCESS_USER_CODE, GDT_FLAGS_LONG_CODE);

    tss.rsp0 = syscall_stack_top;
    tss.iopb_offset = sizeof(tss);
    set_tss_descriptor(6u, (uintptr_t)&tss, sizeof(tss) - 1u);
    load_gdt_and_tss();

    scdk_status_t status = scdk_syscall_init(syscall_stack_top);
    if (status != SCDK_OK) {
        return status;
    }

    usermode_initialized = true;
    return SCDK_OK;
}

static scdk_status_t run_stub_in_aspace(scdk_cap_t aspace,
                                        uint64_t hhdm_offset) {
    uint64_t boot_root = 0;
    uint64_t code_phys = 0;
    uint64_t stack_phys = 0;
    scdk_cap_t console_endpoint = 0;
    uintptr_t code_page;
    uintptr_t stack_page;
    size_t stub_size;
    uint64_t syscall_stack_top;
    scdk_status_t status;

    if (aspace == 0 || hhdm_offset == 0u) {
        return SCDK_ERR_INVAL;
    }

    syscall_stack_top = (uint64_t)(uintptr_t)(syscall_stack + sizeof(syscall_stack));
    status = scdk_usermode_init(syscall_stack_top);
    if (status != SCDK_OK) {
        return status;
    }

    scdk_log_write("user", "preparing test address space");

    status = scdk_vmm_current_root(&boot_root);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_service_lookup(SCDK_SERVICE_CONSOLE, &console_endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_page_alloc(&code_phys);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_page_alloc(&stack_phys);
    if (status != SCDK_OK) {
        (void)scdk_page_free(code_phys);
        return status;
    }

    code_page = (uintptr_t)(hhdm_offset + code_phys);
    stack_page = (uintptr_t)(hhdm_offset + stack_phys);
    memset((void *)code_page, 0, SCDK_PAGE_SIZE);
    memset((void *)stack_page, 0, SCDK_PAGE_SIZE);

    stub_size = (size_t)(scdk_user_test_stub_end - scdk_user_test_stub_start);
    if (stub_size == 0u || stub_size > SCDK_PAGE_SIZE) {
        (void)scdk_page_free(stack_phys);
        (void)scdk_page_free(code_phys);
        return SCDK_ERR_BOUNDS;
    }
    memcpy((void *)code_page, scdk_user_test_stub_start, stub_size);

    status = scdk_address_space_map(aspace,
                                    SCDK_USER_TEST_CODE_VIRT,
                                    code_phys,
                                    0);
    if (status != SCDK_OK) {
        (void)scdk_page_free(stack_phys);
        (void)scdk_page_free(code_phys);
        return status;
    }

    status = scdk_address_space_map(aspace,
                                    SCDK_USER_TEST_STACK_TOP - SCDK_PAGE_SIZE,
                                    stack_phys,
                                    SCDK_VMM_MAP_WRITE);
    if (status != SCDK_OK) {
        (void)scdk_address_space_unmap(aspace, SCDK_USER_TEST_CODE_VIRT);
        (void)scdk_page_free(stack_phys);
        (void)scdk_page_free(code_phys);
        return status;
    }

    status = scdk_address_space_activate(aspace);
    if (status != SCDK_OK) {
        return status;
    }

    scdk_log_write("user", "entering ring3");
    scdk_enter_usermode_asm(SCDK_USER_TEST_CODE_VIRT,
                            SCDK_USER_TEST_STACK_TOP,
                            console_endpoint);

    status = scdk_vmm_activate_root(boot_root);
    if (status != SCDK_OK) {
        return status;
    }

    if (!usermode_initialized ||
        !scdk_syscall_user_exited() ||
        !scdk_syscall_endpoint_call_passed()) {
        return SCDK_ERR_BUSY;
    }

    scdk_log_write("user", "returned or exited");

    (void)scdk_address_space_unmap(aspace, SCDK_USER_TEST_STACK_TOP - SCDK_PAGE_SIZE);
    (void)scdk_address_space_unmap(aspace, SCDK_USER_TEST_CODE_VIRT);
    (void)scdk_page_free(stack_phys);
    (void)scdk_page_free(code_phys);

    return SCDK_OK;
}

scdk_status_t scdk_usermode_run_task_test(scdk_cap_t aspace,
                                          scdk_cap_t thread,
                                          uint64_t hhdm_offset) {
    scdk_status_t status;

    status = scdk_cap_check(thread, SCDK_RIGHT_READ, SCDK_OBJ_THREAD, 0);
    if (status != SCDK_OK) {
        return status;
    }

    return run_stub_in_aspace(aspace, hhdm_offset);
}

scdk_status_t scdk_usermode_run_builtin_test(uint64_t hhdm_offset) {
    scdk_cap_t aspace = 0;
    scdk_status_t status;

    if (hhdm_offset == 0u) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_address_space_create(&aspace);
    if (status != SCDK_OK) {
        return status;
    }

    return run_stub_in_aspace(aspace, hhdm_offset);
}
