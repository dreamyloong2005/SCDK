// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_SELFTEST_H
#define SCDK_SELFTEST_H

#include <stdint.h>

#include <scdk/types.h>

struct limine_memmap_response;

/*
 * Control-plane diagnostic: provide boot resources needed by PMM/VMM tests.
 * This does not transfer ownership of Limine structures.
 */
void scdk_selftest_set_boot_context(const struct limine_memmap_response *memmap,
                                    uint64_t hhdm_offset);

/*
 * Control-plane diagnostic: run the boot-time regression suite for the current
 * SCDK core interfaces.
 */
scdk_status_t scdk_run_core_selftests(void);

#endif
