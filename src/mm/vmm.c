// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/mm.h>

#include <stddef.h>

#include <scdk/log.h>
#include <scdk/string.h>

#define X86_PTE_PRESENT  (1ull << 0)
#define X86_PTE_WRITABLE (1ull << 1)
#define X86_PTE_USER     (1ull << 2)
#define X86_PTE_HUGE     (1ull << 7)
#define X86_PTE_ADDR     0x000ffffffffff000ull
#define X86_CR3_ADDR     0x000ffffffffff000ull

static uint64_t active_cr3_phys;
static uint64_t active_hhdm_offset;
static bool vmm_initialized;

static inline uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static inline void invlpg(uint64_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static bool is_canonical(uint64_t virt) {
    uint64_t top = virt >> 48u;
    return top == 0u || top == 0xffffu;
}

static bool is_page_aligned(uint64_t value) {
    return (value & (SCDK_PAGE_SIZE - 1u)) == 0u;
}

static uint64_t pml4_index(uint64_t virt) {
    return (virt >> 39u) & 0x1ffu;
}

static uint64_t pdpt_index(uint64_t virt) {
    return (virt >> 30u) & 0x1ffu;
}

static uint64_t pd_index(uint64_t virt) {
    return (virt >> 21u) & 0x1ffu;
}

static uint64_t pt_index(uint64_t virt) {
    return (virt >> 12u) & 0x1ffu;
}

static uint64_t *phys_to_hhdm(uint64_t phys) {
    return (uint64_t *)(uintptr_t)(active_hhdm_offset + phys);
}

static scdk_status_t validate_vmm_address(uint64_t virt) {
    if (!vmm_initialized) {
        return SCDK_ERR_NOTSUP;
    }

    if (!is_page_aligned(virt) || !is_canonical(virt)) {
        return SCDK_ERR_INVAL;
    }

    return SCDK_OK;
}

static scdk_status_t allocate_table(uint64_t *out_phys) {
    uint64_t phys = 0;
    scdk_status_t status = scdk_page_alloc(&phys);

    if (status != SCDK_OK) {
        return status;
    }

    memset(phys_to_hhdm(phys), 0, SCDK_PAGE_SIZE);
    *out_phys = phys;
    return SCDK_OK;
}

static scdk_status_t next_table(uint64_t *table,
                                uint64_t index,
                                bool create,
                                uint64_t **out_next) {
    uint64_t entry = table[index];

    if ((entry & X86_PTE_PRESENT) == 0u) {
        if (!create) {
            return SCDK_ERR_NOENT;
        }

        uint64_t new_table_phys = 0;
        scdk_status_t status = allocate_table(&new_table_phys);
        if (status != SCDK_OK) {
            return status;
        }

        entry = new_table_phys | X86_PTE_PRESENT | X86_PTE_WRITABLE;
        table[index] = entry;
    }

    if ((entry & X86_PTE_HUGE) != 0u) {
        return SCDK_ERR_BUSY;
    }

    *out_next = phys_to_hhdm(entry & X86_PTE_ADDR);
    return SCDK_OK;
}

static scdk_status_t leaf_table_for(uint64_t virt,
                                    bool create,
                                    uint64_t **out_pt) {
    uint64_t *pml4 = phys_to_hhdm(active_cr3_phys);
    uint64_t *pdpt = 0;
    uint64_t *pd = 0;
    uint64_t *pt = 0;
    scdk_status_t status;

    status = next_table(pml4, pml4_index(virt), create, &pdpt);
    if (status != SCDK_OK) {
        return status;
    }

    status = next_table(pdpt, pdpt_index(virt), create, &pd);
    if (status != SCDK_OK) {
        return status;
    }

    status = next_table(pd, pd_index(virt), create, &pt);
    if (status != SCDK_OK) {
        return status;
    }

    *out_pt = pt;
    return SCDK_OK;
}

scdk_status_t scdk_vmm_init(uint64_t hhdm_offset) {
    if (hhdm_offset == 0u) {
        return SCDK_ERR_INVAL;
    }

    active_hhdm_offset = hhdm_offset;
    active_cr3_phys = read_cr3() & X86_CR3_ADDR;
    if (active_cr3_phys == 0u) {
        return SCDK_ERR_NOENT;
    }

    vmm_initialized = true;
    scdk_log_info("vmm current cr3: 0x%llx",
                  (unsigned long long)active_cr3_phys);
    return SCDK_OK;
}

scdk_status_t scdk_vmm_current_root(uint64_t *out_cr3_phys) {
    if (out_cr3_phys == 0) {
        return SCDK_ERR_INVAL;
    }

    if (!vmm_initialized) {
        return SCDK_ERR_NOTSUP;
    }

    *out_cr3_phys = active_cr3_phys;
    return SCDK_OK;
}

scdk_status_t scdk_vmm_map_page(uint64_t virt,
                                uint64_t phys,
                                uint64_t flags) {
    uint64_t *pt = 0;
    uint64_t pte_flags = X86_PTE_PRESENT;
    scdk_status_t status;

    status = validate_vmm_address(virt);
    if (status != SCDK_OK) {
        return status;
    }

    if (!is_page_aligned(phys) || (flags & ~(SCDK_VMM_MAP_WRITE | SCDK_VMM_MAP_USER)) != 0u) {
        return SCDK_ERR_INVAL;
    }

    status = leaf_table_for(virt, true, &pt);
    if (status != SCDK_OK) {
        return status;
    }

    uint64_t index = pt_index(virt);
    if ((pt[index] & X86_PTE_PRESENT) != 0u) {
        return SCDK_ERR_BUSY;
    }

    if ((flags & SCDK_VMM_MAP_WRITE) != 0u) {
        pte_flags |= X86_PTE_WRITABLE;
    }

    if ((flags & SCDK_VMM_MAP_USER) != 0u) {
        pte_flags |= X86_PTE_USER;
    }

    pt[index] = (phys & X86_PTE_ADDR) | pte_flags;
    invlpg(virt);
    return SCDK_OK;
}

scdk_status_t scdk_vmm_unmap_page(uint64_t virt) {
    uint64_t *pt = 0;
    scdk_status_t status = validate_vmm_address(virt);

    if (status != SCDK_OK) {
        return status;
    }

    status = leaf_table_for(virt, false, &pt);
    if (status != SCDK_OK) {
        return status;
    }

    uint64_t index = pt_index(virt);
    if ((pt[index] & X86_PTE_PRESENT) == 0u) {
        return SCDK_ERR_NOENT;
    }

    pt[index] = 0;
    invlpg(virt);
    return SCDK_OK;
}

scdk_status_t scdk_vmm_virt_to_phys(uint64_t virt,
                                    uint64_t *out_phys,
                                    uint64_t *out_flags) {
    uint64_t *pt = 0;
    scdk_status_t status = validate_vmm_address(virt & ~(SCDK_PAGE_SIZE - 1u));

    if (status != SCDK_OK) {
        return status;
    }

    if (out_phys == 0) {
        return SCDK_ERR_INVAL;
    }

    status = leaf_table_for(virt, false, &pt);
    if (status != SCDK_OK) {
        return status;
    }

    uint64_t entry = pt[pt_index(virt)];
    if ((entry & X86_PTE_PRESENT) == 0u) {
        return SCDK_ERR_NOENT;
    }

    *out_phys = (entry & X86_PTE_ADDR) | (virt & (SCDK_PAGE_SIZE - 1u));
    if (out_flags != 0) {
        uint64_t flags = 0;
        if ((entry & X86_PTE_WRITABLE) != 0u) {
            flags |= SCDK_VMM_MAP_WRITE;
        }
        if ((entry & X86_PTE_USER) != 0u) {
            flags |= SCDK_VMM_MAP_USER;
        }
        *out_flags = flags;
    }

    return SCDK_OK;
}

void scdk_vmm_page_fault_placeholder(uint64_t fault_addr,
                                     uint64_t error_code) {
    scdk_log_write("fault", "page fault placeholder addr=0x%llx error=0x%llx",
                   (unsigned long long)fault_addr,
                   (unsigned long long)error_code);
}
