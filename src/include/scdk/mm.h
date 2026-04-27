// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_MM_H
#define SCDK_MM_H

#include <stdbool.h>
#include <stdint.h>

#include <limine.h>

#include <scdk/types.h>

#define SCDK_PAGE_SIZE 4096ull
#define SCDK_PMM_MAX_PAGES 262144u
#define SCDK_PMM_MAX_REGIONS 128u

#define SCDK_VMM_MAP_WRITE (1ull << 0)
#define SCDK_VMM_MAP_USER  (1ull << 1)

struct scdk_pmm_stats {
    uint64_t managed_pages;
    uint64_t free_pages;
    uint64_t allocated_pages;
    uint64_t usable_regions;
    uint64_t reserved_regions;
};

/*
 * Control-plane: initialize the bootstrap physical page allocator.
 * Only Limine USABLE ranges become allocatable; all other map types stay
 * reserved until later memory-management milestones decide otherwise.
 */
scdk_status_t scdk_pmm_init(const struct limine_memmap_response *memmap);

/*
 * Data-plane prototype: allocate one physical 4 KiB page.
 * Requires no capability during bootstrap; future callers should hold memory
 * allocation authority.
 */
scdk_status_t scdk_page_alloc(uint64_t *out_phys);

/*
 * Data-plane prototype: return one physical 4 KiB page to the PMM.
 * Requires the page to have been allocated from a Limine USABLE range.
 */
scdk_status_t scdk_page_free(uint64_t phys);

/*
 * Control-plane diagnostic: test whether a physical page belongs to a usable
 * Limine range managed by the PMM.
 */
bool scdk_pmm_is_usable_page(uint64_t phys);

/*
 * Control-plane diagnostic: test whether a physical page overlaps a reserved
 * Limine range or falls outside all usable ranges.
 */
bool scdk_pmm_is_reserved_page(uint64_t phys);

/*
 * Control-plane diagnostic: read allocator counters for boot self-tests.
 */
void scdk_pmm_get_stats(struct scdk_pmm_stats *out_stats);

/*
 * Control-plane: initialize the current x86_64 page-table manager.
 * Requires PMM to be initialized and HHDM to be available.
 */
scdk_status_t scdk_vmm_init(uint64_t hhdm_offset);

/*
 * Control-plane diagnostic: report the active CR3 physical address.
 */
scdk_status_t scdk_vmm_current_root(uint64_t *out_cr3_phys);

/*
 * Control-plane: create a new x86_64 root page table.
 * The new root starts with an empty user half and copies the current kernel
 * half so kernel code, HHDM, and bootstrap mappings remain reachable.
 */
scdk_status_t scdk_vmm_create_root(uint64_t *out_root_phys);

/*
 * Control-plane: switch the active x86_64 root page table.
 * Intended for address-space activation and boot diagnostics.
 */
scdk_status_t scdk_vmm_activate_root(uint64_t root_phys);

/*
 * Control-plane: map one 4 KiB physical page at a virtual address in the
 * active address space. Missing page-table pages are allocated from PMM.
 */
scdk_status_t scdk_vmm_map_page(uint64_t virt,
                                uint64_t phys,
                                uint64_t flags);

/*
 * Control-plane: map one 4 KiB physical page in a specific root page table.
 * Missing page-table pages are allocated from PMM.
 */
scdk_status_t scdk_vmm_map_page_in_root(uint64_t root_phys,
                                        uint64_t virt,
                                        uint64_t phys,
                                        uint64_t flags);

/*
 * Control-plane: remove one 4 KiB mapping from the active address space.
 */
scdk_status_t scdk_vmm_unmap_page(uint64_t virt);

/*
 * Control-plane: remove one 4 KiB mapping from a specific root page table.
 */
scdk_status_t scdk_vmm_unmap_page_in_root(uint64_t root_phys,
                                          uint64_t virt);

/*
 * Control-plane diagnostic: translate a mapped virtual page through the
 * current page tables.
 */
scdk_status_t scdk_vmm_virt_to_phys(uint64_t virt,
                                    uint64_t *out_phys,
                                    uint64_t *out_flags);

/*
 * Control-plane diagnostic: translate a virtual address through a specific
 * root page table.
 */
scdk_status_t scdk_vmm_virt_to_phys_in_root(uint64_t root_phys,
                                            uint64_t virt,
                                            uint64_t *out_phys,
                                            uint64_t *out_flags);

/*
 * Fault-path placeholder: log a future page-fault event shape.
 */
void scdk_vmm_page_fault_placeholder(uint64_t fault_addr,
                                     uint64_t error_code);

#endif
