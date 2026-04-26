// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/mm.h>

#include <stddef.h>

#include <scdk/log.h>

struct scdk_pmm_region {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

static uint64_t page_stack[SCDK_PMM_MAX_PAGES];
static uint64_t free_pages;
static uint64_t managed_pages;
static uint64_t allocated_pages;

static struct scdk_pmm_region usable_regions[SCDK_PMM_MAX_REGIONS];
static struct scdk_pmm_region reserved_regions[SCDK_PMM_MAX_REGIONS];
static uint32_t usable_region_count;
static uint32_t reserved_region_count;
static bool pmm_initialized;

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1u);
}

static const char *memmap_type_name(uint64_t type) {
    switch (type) {
    case LIMINE_MEMMAP_USABLE:
        return "usable";
    case LIMINE_MEMMAP_RESERVED:
        return "reserved";
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        return "acpi-reclaimable";
    case LIMINE_MEMMAP_ACPI_NVS:
        return "acpi-nvs";
    case LIMINE_MEMMAP_BAD_MEMORY:
        return "bad-memory";
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        return "bootloader-reclaimable";
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        return "kernel-and-modules";
    case LIMINE_MEMMAP_FRAMEBUFFER:
        return "framebuffer";
    case LIMINE_MEMMAP_RESERVED_MAPPED:
        return "reserved-mapped";
    default:
        return "unknown";
    }
}

static bool range_contains_page(const struct scdk_pmm_region *region,
                                uint64_t phys) {
    uint64_t end = region->base + region->length;

    if (end < region->base) {
        return false;
    }

    return phys >= region->base && phys + SCDK_PAGE_SIZE <= end;
}

static bool page_overlaps_region(const struct scdk_pmm_region *region,
                                 uint64_t phys) {
    uint64_t page_end = phys + SCDK_PAGE_SIZE;
    uint64_t region_end = region->base + region->length;

    if (page_end < phys || region_end < region->base) {
        return true;
    }

    return phys < region_end && page_end > region->base;
}

static void record_region(struct scdk_pmm_region *regions,
                          uint32_t *count,
                          uint64_t base,
                          uint64_t length,
                          uint64_t type) {
    if (*count >= SCDK_PMM_MAX_REGIONS) {
        return;
    }

    regions[*count].base = base;
    regions[*count].length = length;
    regions[*count].type = type;
    (*count)++;
}

static bool page_stack_contains(uint64_t phys) {
    for (uint64_t i = 0; i < free_pages; i++) {
        if (page_stack[i] == phys) {
            return true;
        }
    }

    return false;
}

static scdk_status_t collect_usable_region(uint64_t base, uint64_t end) {
    for (uint64_t page = base; page + SCDK_PAGE_SIZE <= end; page += SCDK_PAGE_SIZE) {
        if (free_pages >= SCDK_PMM_MAX_PAGES) {
            scdk_log_warn("pmm page stack full; remaining usable pages ignored");
            return SCDK_ERR_BUSY;
        }

        page_stack[free_pages++] = page;
        managed_pages++;
    }

    return SCDK_OK;
}

scdk_status_t scdk_pmm_init(const struct limine_memmap_response *memmap) {
    if (memmap == 0 || memmap->entries == 0 || memmap->entry_count == 0u) {
        return SCDK_ERR_INVAL;
    }

    free_pages = 0;
    managed_pages = 0;
    allocated_pages = 0;
    usable_region_count = 0;
    reserved_region_count = 0;
    pmm_initialized = false;

    scdk_log_write("test", "pmm init start");

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        const struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry == 0 || entry->length == 0u) {
            continue;
        }

        uint64_t raw_end = entry->base + entry->length;
        if (raw_end < entry->base) {
            scdk_log_warn("pmm ignoring wrapped memmap entry %llu", (unsigned long long)i);
            continue;
        }

        uint64_t base = align_up(entry->base, SCDK_PAGE_SIZE);
        uint64_t end = align_down(raw_end, SCDK_PAGE_SIZE);
        if (end <= base) {
            continue;
        }

        uint64_t pages = (end - base) / SCDK_PAGE_SIZE;
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            record_region(usable_regions,
                          &usable_region_count,
                          base,
                          end - base,
                          entry->type);
            scdk_log_info("pmm usable: 0x%llx-0x%llx pages %llu",
                          (unsigned long long)base,
                          (unsigned long long)end,
                          (unsigned long long)pages);

            scdk_status_t status = collect_usable_region(base, end);
            if (status != SCDK_OK) {
                break;
            }
        } else {
            record_region(reserved_regions,
                          &reserved_region_count,
                          base,
                          end - base,
                          entry->type);
            scdk_log_info("pmm reserve %s: 0x%llx-0x%llx pages %llu",
                          memmap_type_name(entry->type),
                          (unsigned long long)base,
                          (unsigned long long)end,
                          (unsigned long long)pages);
        }
    }

    if (managed_pages == 0u) {
        return SCDK_ERR_NOMEM;
    }

    pmm_initialized = true;
    scdk_log_info("pmm managed pages: %llu",
                  (unsigned long long)managed_pages);
    return SCDK_OK;
}

scdk_status_t scdk_page_alloc(uint64_t *out_phys) {
    if (out_phys == 0) {
        return SCDK_ERR_INVAL;
    }

    if (!pmm_initialized) {
        return SCDK_ERR_NOTSUP;
    }

    if (free_pages == 0u) {
        return SCDK_ERR_NOMEM;
    }

    *out_phys = page_stack[--free_pages];
    allocated_pages++;
    return SCDK_OK;
}

scdk_status_t scdk_page_free(uint64_t phys) {
    if (!pmm_initialized) {
        return SCDK_ERR_NOTSUP;
    }

    if ((phys & (SCDK_PAGE_SIZE - 1u)) != 0u) {
        return SCDK_ERR_INVAL;
    }

    if (!scdk_pmm_is_usable_page(phys) || scdk_pmm_is_reserved_page(phys)) {
        return SCDK_ERR_BOUNDS;
    }

    if (page_stack_contains(phys)) {
        return SCDK_ERR_BUSY;
    }

    if (free_pages >= SCDK_PMM_MAX_PAGES) {
        return SCDK_ERR_NOMEM;
    }

    page_stack[free_pages++] = phys;
    if (allocated_pages > 0u) {
        allocated_pages--;
    }

    return SCDK_OK;
}

bool scdk_pmm_is_usable_page(uint64_t phys) {
    if ((phys & (SCDK_PAGE_SIZE - 1u)) != 0u) {
        return false;
    }

    for (uint32_t i = 0; i < usable_region_count; i++) {
        if (range_contains_page(&usable_regions[i], phys)) {
            return true;
        }
    }

    return false;
}

bool scdk_pmm_is_reserved_page(uint64_t phys) {
    if ((phys & (SCDK_PAGE_SIZE - 1u)) != 0u) {
        return true;
    }

    for (uint32_t i = 0; i < reserved_region_count; i++) {
        if (page_overlaps_region(&reserved_regions[i], phys)) {
            return true;
        }
    }

    return !scdk_pmm_is_usable_page(phys);
}

void scdk_pmm_get_stats(struct scdk_pmm_stats *out_stats) {
    if (out_stats == 0) {
        return;
    }

    out_stats->managed_pages = managed_pages;
    out_stats->free_pages = free_pages;
    out_stats->allocated_pages = allocated_pages;
    out_stats->usable_regions = usable_region_count;
    out_stats->reserved_regions = reserved_region_count;
}
