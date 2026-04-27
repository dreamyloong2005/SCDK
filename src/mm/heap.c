// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/heap.h>

#include <stdbool.h>
#include <stdint.h>

#include <scdk/log.h>
#include <scdk/mm.h>
#include <scdk/object.h>
#include <scdk/string.h>

#define SCDK_HEAP_ALIGN 16ull
#define SCDK_HEAP_MAGIC 0x5343444b48454150ull
#define SCDK_HEAP_MIN_SPLIT 16ull
#define SCDK_HEAP_INITIAL_PAGES 4ull

struct scdk_heap_block {
    uint64_t magic;
    size_t size;
    bool free;
    struct scdk_heap_block *prev;
    struct scdk_heap_block *next;
};

static struct scdk_heap_block *heap_head;
static struct scdk_heap_block *heap_tail;
static uintptr_t heap_commit_end;
static bool heap_initialized;

static size_t align_up_size(size_t value, size_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static size_t heap_header_size(void) {
    return align_up_size(sizeof(struct scdk_heap_block), SCDK_HEAP_ALIGN);
}

static bool checked_add_size(size_t a, size_t b, size_t *out) {
    if (out == 0 || a > ((size_t)-1) - b) {
        return false;
    }

    *out = a + b;
    return true;
}

static bool checked_add_uintptr(uintptr_t a, uintptr_t b, uintptr_t *out) {
    if (out == 0 || a > ((uintptr_t)-1) - b) {
        return false;
    }

    *out = a + b;
    return true;
}

static bool ptr_in_heap(const void *ptr) {
    uintptr_t value = (uintptr_t)ptr;

    return value >= SCDK_HEAP_BASE && value < heap_commit_end;
}

static void *block_payload(struct scdk_heap_block *block) {
    return (void *)((uintptr_t)block + heap_header_size());
}

static struct scdk_heap_block *payload_block(void *ptr) {
    return (struct scdk_heap_block *)((uintptr_t)ptr - heap_header_size());
}

static void block_insert_tail(struct scdk_heap_block *block) {
    block->prev = heap_tail;
    block->next = 0;

    if (heap_tail != 0) {
        heap_tail->next = block;
    } else {
        heap_head = block;
    }

    heap_tail = block;
}

static void block_remove(struct scdk_heap_block *block) {
    if (block->prev != 0) {
        block->prev->next = block->next;
    } else {
        heap_head = block->next;
    }

    if (block->next != 0) {
        block->next->prev = block->prev;
    } else {
        heap_tail = block->prev;
    }
}

static void block_coalesce_forward(struct scdk_heap_block *block) {
    struct scdk_heap_block *next = block->next;
    uintptr_t block_end = (uintptr_t)block + heap_header_size() + block->size;

    if (next == 0 || !next->free || (uintptr_t)next != block_end) {
        return;
    }

    block->size += heap_header_size() + next->size;
    block_remove(next);
}

static void block_split(struct scdk_heap_block *block, size_t requested) {
    size_t header = heap_header_size();

    if (block->size < requested + header + SCDK_HEAP_MIN_SPLIT) {
        return;
    }

    uintptr_t new_addr = (uintptr_t)block + header + requested;
    struct scdk_heap_block *new_block = (struct scdk_heap_block *)new_addr;

    new_block->magic = SCDK_HEAP_MAGIC;
    new_block->size = block->size - requested - header;
    new_block->free = true;
    new_block->prev = block;
    new_block->next = block->next;

    if (block->next != 0) {
        block->next->prev = new_block;
    } else {
        heap_tail = new_block;
    }

    block->next = new_block;
    block->size = requested;
}

static struct scdk_heap_block *find_free_block(size_t size) {
    for (struct scdk_heap_block *block = heap_head; block != 0; block = block->next) {
        if (block->free && block->size >= size) {
            return block;
        }
    }

    return 0;
}

static scdk_status_t map_heap_pages(uintptr_t base, size_t length) {
    size_t mapped = 0;

    while (mapped < length) {
        uint64_t phys = 0;
        scdk_status_t status = scdk_page_alloc(&phys);

        if (status != SCDK_OK) {
            return status;
        }

        status = scdk_vmm_map_page((uint64_t)(base + mapped),
                                   phys,
                                   SCDK_VMM_MAP_WRITE);
        if (status != SCDK_OK) {
            (void)scdk_page_free(phys);

            while (mapped > 0u) {
                mapped -= SCDK_PAGE_SIZE;
                uint64_t mapped_phys = 0;

                if (scdk_vmm_virt_to_phys((uint64_t)(base + mapped),
                                          &mapped_phys,
                                          0) == SCDK_OK) {
                    (void)scdk_vmm_unmap_page((uint64_t)(base + mapped));
                    (void)scdk_page_free(mapped_phys);
                }
            }

            return status;
        }

        mapped += SCDK_PAGE_SIZE;
    }

    return SCDK_OK;
}

static scdk_status_t heap_extend(size_t min_payload) {
    size_t header = heap_header_size();
    size_t needed = 0;
    size_t commit_size = 0;
    uintptr_t new_end = 0;
    struct scdk_heap_block *block;
    scdk_status_t status;

    if (!checked_add_size(header, min_payload, &needed)) {
        return SCDK_ERR_NOMEM;
    }

    commit_size = align_up_size(needed, SCDK_PAGE_SIZE);
    if (commit_size < SCDK_HEAP_INITIAL_PAGES * SCDK_PAGE_SIZE) {
        commit_size = SCDK_HEAP_INITIAL_PAGES * SCDK_PAGE_SIZE;
    }

    if (!checked_add_uintptr(heap_commit_end, commit_size, &new_end) ||
        new_end > SCDK_HEAP_BASE + SCDK_HEAP_SIZE) {
        return SCDK_ERR_NOMEM;
    }

    status = map_heap_pages(heap_commit_end, commit_size);
    if (status != SCDK_OK) {
        return status;
    }

    block = (struct scdk_heap_block *)heap_commit_end;
    block->magic = SCDK_HEAP_MAGIC;
    block->size = commit_size - header;
    block->free = true;
    block_insert_tail(block);

    heap_commit_end = new_end;

    if (block->prev != 0 && block->prev->free) {
        block = block->prev;
        block_coalesce_forward(block);
    }

    return SCDK_OK;
}

scdk_status_t scdk_heap_init(void) {
    scdk_status_t status;

    if (heap_initialized) {
        return SCDK_ERR_BUSY;
    }

    heap_head = 0;
    heap_tail = 0;
    heap_commit_end = SCDK_HEAP_BASE;

    status = heap_extend(0);
    if (status != SCDK_OK) {
        return status;
    }

    heap_initialized = true;
    scdk_log_write("heap", "init ok");
    return SCDK_OK;
}

void *scdk_kalloc(size_t size) {
    size_t requested;
    size_t header = heap_header_size();
    struct scdk_heap_block *block;

    if (!heap_initialized || size == 0u) {
        return 0;
    }

    if (size > SCDK_HEAP_SIZE - header) {
        return 0;
    }

    requested = align_up_size(size, SCDK_HEAP_ALIGN);
    if (requested < size || requested > SCDK_HEAP_SIZE - header) {
        return 0;
    }

    block = find_free_block(requested);
    if (block == 0) {
        if (heap_extend(requested) != SCDK_OK) {
            return 0;
        }

        block = find_free_block(requested);
        if (block == 0) {
            return 0;
        }
    }

    block_split(block, requested);
    block->free = false;
    return block_payload(block);
}

void *scdk_kzalloc(size_t size) {
    void *ptr = scdk_kalloc(size);

    if (ptr != 0) {
        memset(ptr, 0, size);
    }

    return ptr;
}

void scdk_kfree(void *ptr) {
    struct scdk_heap_block *block;

    if (ptr == 0 || !heap_initialized || !ptr_in_heap(ptr)) {
        return;
    }

    block = payload_block(ptr);
    if (!ptr_in_heap(block) || block->magic != SCDK_HEAP_MAGIC || block->free) {
        return;
    }

    block->free = true;
    block_coalesce_forward(block);
    if (block->prev != 0 && block->prev->free) {
        block = block->prev;
        block_coalesce_forward(block);
    }
}

void *scdk_alloc_object_storage(uint32_t object_type, size_t size) {
    if (object_type == SCDK_OBJ_NONE || size == 0u) {
        return 0;
    }

    return scdk_kzalloc(size);
}
