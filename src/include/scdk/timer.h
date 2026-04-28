// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_TIMER_H
#define SCDK_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#include <scdk/types.h>

/*
 * Control-plane: initialize the single-core PIT timer and IRQ0 gate.
 */
scdk_status_t scdk_timer_init(uint32_t hz);

/*
 * Diagnostic: read the monotonic timer tick counter.
 */
uint64_t scdk_timer_ticks(void);

/*
 * Scheduler control-plane: enable or disable timer-driven preemption.
 */
void scdk_timer_enable_preemption(void);
void scdk_timer_disable_preemption(void);

/*
 * Diagnostic: report whether at least one timer interrupt was observed.
 */
bool scdk_timer_tick_seen(void);

/*
 * IRQ entry point called by the x86_64 timer assembly stub.
 */
void scdk_timer_interrupt(void);

#endif
