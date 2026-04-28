// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/timer.h>

#include <scdk/fault.h>
#include <scdk/scheduler.h>

#define PIT_BASE_HZ 1193182u
#define PIT_CHANNEL0 0x40u
#define PIT_COMMAND  0x43u
#define PIT_MODE_RATE_LOHI 0x34u

#define PIC1_COMMAND 0x20u
#define PIC1_DATA    0x21u
#define PIC2_COMMAND 0xa0u
#define PIC2_DATA    0xa1u
#define PIC_EOI      0x20u

#define PIC_ICW1_INIT 0x10u
#define PIC_ICW1_ICW4 0x01u
#define PIC_ICW4_8086 0x01u

#define IRQ0_VECTOR 32u

extern void scdk_timer_irq_entry(void);

static volatile uint64_t timer_ticks;
static volatile bool timer_preemption_enabled;
static bool timer_initialized;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline void io_wait(void) {
    outb(0x80u, 0);
}

static inline void sti(void) {
    __asm__ volatile ("sti" ::: "memory");
}

static void pic_remap_for_timer(void) {
    outb(PIC1_DATA, 0xffu);
    outb(PIC2_DATA, 0xffu);

    outb(PIC1_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 0x20u);
    io_wait();
    outb(PIC2_DATA, 0x28u);
    io_wait();

    outb(PIC1_DATA, 0x04u);
    io_wait();
    outb(PIC2_DATA, 0x02u);
    io_wait();

    outb(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC2_DATA, PIC_ICW4_8086);
    io_wait();

    outb(PIC1_DATA, 0xfeu);
    outb(PIC2_DATA, 0xffu);
}

static scdk_status_t pit_program(uint32_t hz) {
    uint32_t divisor;

    if (hz == 0u || hz > PIT_BASE_HZ) {
        return SCDK_ERR_INVAL;
    }

    divisor = PIT_BASE_HZ / hz;
    if (divisor == 0u || divisor > 0xffffu) {
        return SCDK_ERR_INVAL;
    }

    outb(PIT_COMMAND, PIT_MODE_RATE_LOHI);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xffu));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8u) & 0xffu));
    return SCDK_OK;
}

scdk_status_t scdk_timer_init(uint32_t hz) {
    scdk_status_t status;

    if (timer_initialized) {
        return SCDK_OK;
    }

    status = scdk_fault_init();
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_fault_install_gate(IRQ0_VECTOR, scdk_timer_irq_entry);
    if (status != SCDK_OK) {
        return status;
    }

    pic_remap_for_timer();
    status = pit_program(hz);
    if (status != SCDK_OK) {
        return status;
    }

    timer_ticks = 0;
    timer_preemption_enabled = false;
    timer_initialized = true;
    return SCDK_OK;
}

uint64_t scdk_timer_ticks(void) {
    return timer_ticks;
}

void scdk_timer_enable_preemption(void) {
    timer_preemption_enabled = true;
    sti();
}

void scdk_timer_disable_preemption(void) {
    timer_preemption_enabled = false;
}

bool scdk_timer_tick_seen(void) {
    return timer_ticks != 0u;
}

void scdk_timer_interrupt(void) {
    timer_ticks++;
    outb(PIC1_COMMAND, PIC_EOI);

    if (timer_preemption_enabled) {
        sti();
        scdk_yield();
    }
}
