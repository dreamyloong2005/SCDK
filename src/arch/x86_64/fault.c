// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/fault.h>

#include <scdk/log.h>
#include <scdk/panic.h>
#include <scdk/string.h>
#include <scdk/syscall.h>
#include <scdk/task.h>

#define IDT_ENTRIES 256u
#define IDT_KERNEL_CODE_SEL 0x08u
#define IDT_GATE_INTERRUPT 0x8eu

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern void scdk_isr_invalid_opcode(void);
extern void scdk_isr_general_protection(void);
extern void scdk_isr_page_fault(void);

static struct idt_entry idt[IDT_ENTRIES];
static bool fault_initialized;
static bool saw_user_page_fault;
static bool saw_invalid_syscall;
static bool saw_bad_user_pointer;

static void lidt(const struct idt_pointer *pointer) {
    __asm__ volatile ("lidt %0" : : "m"(*pointer) : "memory");
}

static uint64_t read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static void set_gate(uint8_t vector, void (*handler)(void)) {
    uint64_t address = (uint64_t)(uintptr_t)handler;

    idt[vector].offset_low = (uint16_t)address;
    idt[vector].selector = IDT_KERNEL_CODE_SEL;
    idt[vector].ist = 0;
    idt[vector].type_attr = IDT_GATE_INTERRUPT;
    idt[vector].offset_mid = (uint16_t)(address >> 16u);
    idt[vector].offset_high = (uint32_t)(address >> 32u);
    idt[vector].zero = 0;
}

static bool frame_from_user(const struct scdk_fault_frame *frame) {
    return frame != 0 && (frame->cs & 3u) == 3u;
}

static void mark_user_fault(scdk_status_t status) {
    scdk_status_t task_status = scdk_task_fault_current((int)status);

    if (task_status != SCDK_OK) {
        scdk_log_write("fault", "task killed");
    }

    scdk_syscall_mark_user_fault(status);
}

scdk_status_t scdk_fault_init(void) {
    struct idt_pointer pointer;

    if (fault_initialized) {
        return SCDK_OK;
    }

    memset(idt, 0, sizeof(idt));
    set_gate(SCDK_FAULT_INVALID_OPCODE, scdk_isr_invalid_opcode);
    set_gate(SCDK_FAULT_GENERAL_PROTECTION, scdk_isr_general_protection);
    set_gate(SCDK_FAULT_PAGE, scdk_isr_page_fault);

    pointer.limit = sizeof(idt) - 1u;
    pointer.base = (uint64_t)(uintptr_t)idt;
    lidt(&pointer);

    fault_initialized = true;
    return SCDK_OK;
}

scdk_status_t scdk_fault_install_gate(uint8_t vector, void (*handler)(void)) {
    struct idt_pointer pointer;

    if (handler == 0) {
        return SCDK_ERR_INVAL;
    }

    if (!fault_initialized) {
        scdk_status_t status = scdk_fault_init();
        if (status != SCDK_OK) {
            return status;
        }
    }

    set_gate(vector, handler);
    pointer.limit = sizeof(idt) - 1u;
    pointer.base = (uint64_t)(uintptr_t)idt;
    lidt(&pointer);
    return SCDK_OK;
}

uint64_t scdk_fault_dispatch(struct scdk_fault_frame *frame) {
    uint64_t fault_addr = 0;

    if (frame == 0) {
        scdk_panic("fault dispatch without frame");
    }

    if (!frame_from_user(frame)) {
        scdk_panic("kernel fault vector=%llu error=0x%llx rip=0x%llx",
                   (unsigned long long)frame->vector,
                   (unsigned long long)frame->error_code,
                   (unsigned long long)frame->rip);
    }

    switch (frame->vector) {
    case SCDK_FAULT_PAGE:
        fault_addr = read_cr2();
        saw_user_page_fault = true;
        scdk_log_write("fault", "user page fault");
        scdk_log_info("user fault addr=0x%llx rip=0x%llx error=0x%llx",
                      (unsigned long long)fault_addr,
                      (unsigned long long)frame->rip,
                      (unsigned long long)frame->error_code);
        break;
    case SCDK_FAULT_INVALID_OPCODE:
        scdk_log_write("fault", "user invalid opcode");
        break;
    case SCDK_FAULT_GENERAL_PROTECTION:
        scdk_log_write("fault", "user general protection fault");
        break;
    default:
        scdk_log_write("fault", "user exception vector %llu",
                       (unsigned long long)frame->vector);
        break;
    }

    mark_user_fault(SCDK_ERR_PERM);
    return 1u;
}

void scdk_fault_handle_invalid_syscall(uint64_t number) {
    saw_invalid_syscall = true;
    scdk_log_write("fault", "invalid syscall handled");
    scdk_log_info("invalid syscall number: %llu",
                  (unsigned long long)number);
    mark_user_fault(SCDK_ERR_NOTSUP);
}

void scdk_fault_handle_bad_user_pointer(uintptr_t user_ptr,
                                        scdk_status_t status) {
    saw_bad_user_pointer = true;
    scdk_log_write("fault", "bad user pointer handled");
    scdk_log_info("bad user pointer: 0x%llx status=%lld",
                  (unsigned long long)user_ptr,
                  (long long)status);
    mark_user_fault(status);
}

void scdk_fault_handle_capability_failure(scdk_status_t status) {
    scdk_log_write("fault", "capability failure handled");
    mark_user_fault(status);
}

void scdk_fault_reset_test_state(void) {
    saw_user_page_fault = false;
    saw_invalid_syscall = false;
    saw_bad_user_pointer = false;
}

bool scdk_fault_saw_user_page_fault(void) {
    return saw_user_page_fault;
}

bool scdk_fault_saw_invalid_syscall(void) {
    return saw_invalid_syscall;
}

bool scdk_fault_saw_bad_user_pointer(void) {
    return saw_bad_user_pointer;
}
