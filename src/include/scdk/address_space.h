// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_ADDRESS_SPACE_H
#define SCDK_ADDRESS_SPACE_H

#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_USER_VIRT_BASE 0x0000000000100000ull
#define SCDK_USER_VIRT_TOP  0x0000800000000000ull
#define SCDK_KERNEL_VIRT_BASE 0xffff800000000000ull

/*
 * Control-plane: create an address-space object and mint its bootstrap cap.
 * The returned cap carries READ/WRITE/MAP/BIND for bootstrap address-space
 * setup. Future callers should hold task or namespace authority.
 */
scdk_status_t scdk_address_space_create(scdk_cap_t *out_cap);

/*
 * Control-plane: map one user page into an address space.
 * Requires SCDK_RIGHT_MAP on an SCDK_OBJ_ADDRESS_SPACE capability. Writable
 * mappings also require SCDK_RIGHT_WRITE.
 */
scdk_status_t scdk_address_space_map(scdk_cap_t aspace_cap,
                                     uintptr_t vaddr,
                                     uintptr_t paddr,
                                     uint64_t flags);

/*
 * Control-plane: unmap one user page from an address space.
 * Requires SCDK_RIGHT_MAP on an SCDK_OBJ_ADDRESS_SPACE capability.
 */
scdk_status_t scdk_address_space_unmap(scdk_cap_t aspace_cap,
                                       uintptr_t vaddr);

/*
 * Control-plane: make an address space active on the current CPU.
 * Requires SCDK_RIGHT_READ on an SCDK_OBJ_ADDRESS_SPACE capability.
 */
scdk_status_t scdk_address_space_activate(scdk_cap_t aspace_cap);

/*
 * Control-plane diagnostic: translate a user virtual address through an
 * address space without activating it.
 */
scdk_status_t scdk_address_space_translate(scdk_cap_t aspace_cap,
                                           uintptr_t vaddr,
                                           uint64_t *out_phys,
                                           uint64_t *out_flags);

#endif
