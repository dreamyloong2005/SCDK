// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_HEAP_H
#define SCDK_HEAP_H

#include <stddef.h>
#include <stdint.h>

#include <scdk/types.h>

#define SCDK_HEAP_BASE 0xffffffffd0000000ull
#define SCDK_HEAP_SIZE (4ull * 1024ull * 1024ull)

/*
 * Control-plane: initialize the bootstrap kernel heap.
 * Requires PMM and VMM to be initialized.
 */
scdk_status_t scdk_heap_init(void);

/*
 * Internal kernel allocation helper.
 * Returned pointers must not become subsystem interfaces across SCDK
 * object/capability/message boundaries.
 */
void *scdk_kalloc(size_t size);

/*
 * Internal kernel allocation helper that zeroes the returned block.
 * Returned pointers follow the same boundary rules as scdk_kalloc().
 */
void *scdk_kzalloc(size_t size);

/*
 * Internal kernel free helper.
 * Does not grant ownership transfer across subsystem boundaries.
 */
void scdk_kfree(void *ptr);

/*
 * Control-plane helper: allocate zeroed private storage for an object payload.
 * The payload may be referenced by object metadata, but external access must
 * still go through capabilities.
 */
void *scdk_alloc_object_storage(uint32_t object_type, size_t size);

#endif
