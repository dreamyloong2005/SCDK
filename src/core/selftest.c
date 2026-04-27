// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/selftest.h>

#include <stdint.h>

#include <scdk/address_space.h>
#include <scdk/capability.h>
#include <scdk/console_service.h>
#include <scdk/endpoint.h>
#include <scdk/grant.h>
#include <scdk/heap.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/mm.h>
#include <scdk/object.h>
#include <scdk/panic.h>
#include <scdk/ring.h>
#include <scdk/scheduler.h>
#include <scdk/service.h>
#include <scdk/task.h>
#include <scdk/thread.h>
#include <scdk/usermode.h>

#define SCDK_VMM_SELFTEST_VIRT 0xffffffffc0000000ull
#define SCDK_ASPACE_SELFTEST_VIRT 0x0000000000400000ull

static const struct limine_memmap_response *selftest_memmap;
static uint64_t selftest_hhdm_offset;

void scdk_selftest_set_boot_context(const struct limine_memmap_response *memmap,
                                    uint64_t hhdm_offset) {
    selftest_memmap = memmap;
    selftest_hhdm_offset = hhdm_offset;
}

static void require_status(const char *name,
                           scdk_status_t actual,
                           scdk_status_t expected) {
    if (actual != expected) {
        scdk_panic("%s failed: got %lld expected %lld",
                   name,
                   (long long)actual,
                   (long long)expected);
    }

    scdk_log_write("test", "%s pass", name);
}

static void run_object_capability_selftest(void) {
    scdk_object_id_t object_id = 0;
    scdk_cap_t cap = 0;
    const struct scdk_object *object = 0;
    const struct scdk_cap_entry *entry = 0;
    scdk_status_t status;

    scdk_log_write("test", "object/capability self-test start");

    status = scdk_object_create(SCDK_OBJ_SERVICE,
                                SCDK_BOOT_CORE,
                                0,
                                0,
                                &object_id);
    require_status("object create", status, SCDK_OK);

    status = scdk_object_lookup(object_id, &object);
    require_status("object lookup", status, SCDK_OK);
    if (object->type != SCDK_OBJ_SERVICE || object->owner_core != SCDK_BOOT_CORE) {
        scdk_panic("object metadata mismatch");
    }
    scdk_log_write("test", "object metadata pass");
    scdk_log_write("test", "object: pass");

    status = scdk_cap_create(object_id,
                             SCDK_RIGHT_READ | SCDK_RIGHT_WRITE,
                             &cap);
    require_status("cap create", status, SCDK_OK);

    status = scdk_cap_lookup(cap, &entry);
    require_status("cap lookup", status, SCDK_OK);
    if (entry->object_id != object_id ||
        entry->object_type != SCDK_OBJ_SERVICE ||
        entry->rights != (SCDK_RIGHT_READ | SCDK_RIGHT_WRITE)) {
        scdk_panic("cap metadata mismatch");
    }
    scdk_log_write("test", "cap metadata pass");

    status = scdk_cap_check(cap, SCDK_RIGHT_READ, SCDK_OBJ_SERVICE, 0);
    require_status("cap allowed right", status, SCDK_OK);

    status = scdk_cap_check(cap, SCDK_RIGHT_EXEC, SCDK_OBJ_SERVICE, 0);
    require_status("cap missing right rejected", status, SCDK_ERR_PERM);

    status = scdk_cap_check(0, SCDK_RIGHT_READ, SCDK_OBJ_SERVICE, 0);
    if (status == SCDK_OK) {
        scdk_panic("invalid cap unexpectedly accepted");
    }
    scdk_log_write("test", "invalid cap rejected pass");
    scdk_log_write("test", "capability: pass");

    scdk_log_write("boot", "capability core initialized");
}

static void run_endpoint_message_selftest(void) {
    scdk_cap_t console_endpoint = 0;
    scdk_cap_t looked_up_endpoint = 0;
    struct scdk_message msg;
    scdk_status_t status;

    scdk_log_write("test", "endpoint/message self-test start");

    status = scdk_console_service_init(&console_endpoint);
    require_status("console service init", status, SCDK_OK);

    status = scdk_log_set_console_endpoint(console_endpoint);
    require_status("console log route", status, SCDK_OK);

    status = scdk_service_lookup(SCDK_SERVICE_CONSOLE, &looked_up_endpoint);
    require_status("console service lookup", status, SCDK_OK);
    if (looked_up_endpoint != console_endpoint) {
        scdk_panic("console service endpoint mismatch");
    }
    scdk_log_write("test", "console service endpoint pass");

    status = scdk_message_init_write(&msg,
                                     0,
                                     0,
                                     "[console] SCDK_MSG_WRITE reached console service",
                                     0);
    require_status("message write init", status, SCDK_OK);

    status = scdk_endpoint_call(looked_up_endpoint, &msg);
    require_status("endpoint message write", status, SCDK_OK);
    if ((scdk_status_t)msg.status != SCDK_OK) {
        scdk_panic("console message returned status %lld", (long long)msg.status);
    }

    status = scdk_message_init_write(&msg,
                                     0,
                                     0,
                                     "invalid endpoint cap should not deliver",
                                     0);
    require_status("invalid message init", status, SCDK_OK);

    status = scdk_endpoint_call(0, &msg);
    require_status("endpoint invalid cap rejected", status, SCDK_ERR_INVAL);

    scdk_log_write("test", "endpoint: pass");
    scdk_log_write("test", "console: pass");
    scdk_log_write("boot", "endpoint/message core initialized");
    scdk_log_write("boot", "console service initialized");
}

static void run_ring_selftest(void) {
    scdk_cap_t ring = 0;
    struct scdk_ring_desc desc;
    struct scdk_completion completion;
    scdk_status_t status;

    scdk_log_write("test", "ring self-test start");

    status = scdk_ring_create(SCDK_BOOT_CORE, 16u, 0, &ring);
    require_status("ring create", status, SCDK_OK);

    for (uint32_t i = 0; i < 16u; i++) {
        desc.op = (uint64_t)i + 1u;
        desc.cap = 0;
        desc.grant = 0;
        desc.offset = (uint64_t)i * 4096u;
        desc.length = 128u + i;
        desc.flags = 0;

        status = scdk_ring_submit(ring, &desc);
        if (status != SCDK_OK) {
            scdk_panic("ring submit %u failed: %lld", i, (long long)status);
        }
    }
    scdk_log_write("test", "ring submitted 16 operations pass");

    status = scdk_ring_submit(ring, &desc);
    require_status("ring full rejected", status, SCDK_ERR_BUSY);

    for (uint32_t i = 0; i < 16u; i++) {
        status = scdk_ring_consume(ring, &desc);
        if (status != SCDK_OK) {
            scdk_panic("ring consume %u failed: %lld", i, (long long)status);
        }

        if (desc.op != (uint64_t)i + 1u ||
            desc.offset != (uint64_t)i * 4096u ||
            desc.length != 128u + i) {
            scdk_panic("ring descriptor %u mismatch", i);
        }

        completion.op = desc.op;
        completion.status = (uint64_t)SCDK_OK;
        completion.result0 = desc.offset + desc.length;
        completion.result1 = i;

        status = scdk_ring_complete(ring, &completion);
        if (status != SCDK_OK) {
            scdk_panic("ring complete %u failed: %lld", i, (long long)status);
        }
    }
    scdk_log_write("test", "ring consumer completed 16 operations pass");

    status = scdk_ring_consume(ring, &desc);
    require_status("ring empty submit queue", status, SCDK_ERR_NOENT);

    for (uint32_t i = 0; i < 16u; i++) {
        uint64_t expected_result0 = ((uint64_t)i * 4096u) + (128u + i);

        status = scdk_ring_poll(ring, &completion);
        if (status != SCDK_OK) {
            scdk_panic("ring poll %u failed: %lld", i, (long long)status);
        }

        if (completion.op != (uint64_t)i + 1u ||
            completion.status != (uint64_t)SCDK_OK ||
            completion.result0 != expected_result0 ||
            completion.result1 != i) {
            scdk_panic("ring completion %u mismatch", i);
        }
    }
    scdk_log_write("test", "ring polled 16 completions pass");

    status = scdk_ring_poll(ring, &completion);
    require_status("ring empty completion queue", status, SCDK_ERR_NOENT);

    scdk_log_write("test", "ring: pass");
    scdk_log_write("boot", "ring core initialized");
}

static void run_grant_selftest(void) {
    static uint8_t grant_buffer[64];

    scdk_cap_t grant = 0;
    void *resolved = 0;
    scdk_status_t status;

    scdk_log_write("test", "grant self-test start");

    for (uint32_t i = 0; i < sizeof(grant_buffer); i++) {
        grant_buffer[i] = (uint8_t)(0xa0u + i);
    }

    status = scdk_grant_create(grant_buffer,
                               sizeof(grant_buffer),
                               SCDK_RIGHT_READ,
                               1u,
                               &grant);
    require_status("grant create", status, SCDK_OK);

    status = scdk_grant_check(grant, SCDK_RIGHT_READ, 8u, 16u);
    require_status("grant valid read range", status, SCDK_OK);

    status = scdk_grant_resolve(grant,
                                SCDK_RIGHT_READ,
                                8u,
                                16u,
                                &resolved);
    require_status("grant service read resolve", status, SCDK_OK);

    const uint8_t *read_view = resolved;
    if (read_view[0] != grant_buffer[8] || read_view[15] != grant_buffer[23]) {
        scdk_panic("grant read data mismatch");
    }
    scdk_log_write("test", "grant service read data pass");

    status = scdk_grant_check(grant, SCDK_RIGHT_READ, 60u, 8u);
    require_status("grant out-of-bounds rejected", status, SCDK_ERR_BOUNDS);

    resolved = 0;
    status = scdk_grant_resolve(grant,
                                SCDK_RIGHT_WRITE,
                                0,
                                1u,
                                &resolved);
    require_status("grant missing write right rejected", status, SCDK_ERR_PERM);
    if (resolved != 0) {
        scdk_panic("grant write resolve returned pointer");
    }

    scdk_log_write("test", "grant: pass");
    scdk_log_write("boot", "grant core initialized");
}

static void run_pmm_selftest(void) {
    struct scdk_pmm_stats before;
    struct scdk_pmm_stats after_alloc;
    struct scdk_pmm_stats after_free;
    uint64_t pages[3];
    scdk_status_t status;

    scdk_log_write("test", "pmm self-test start");

    status = scdk_pmm_init(selftest_memmap);
    require_status("pmm init", status, SCDK_OK);

    scdk_pmm_get_stats(&before);
    if (before.managed_pages == 0u ||
        before.free_pages == 0u ||
        before.usable_regions == 0u ||
        before.reserved_regions == 0u) {
        scdk_panic("pmm stats missing expected memory map data");
    }
    scdk_log_write("test", "pmm stats pass");

    for (uint32_t i = 0; i < 3u; i++) {
        status = scdk_page_alloc(&pages[i]);
        if (status != SCDK_OK) {
            scdk_panic("pmm alloc %u failed: %lld", i, (long long)status);
        }

        if ((pages[i] & (SCDK_PAGE_SIZE - 1u)) != 0u ||
            !scdk_pmm_is_usable_page(pages[i]) ||
            scdk_pmm_is_reserved_page(pages[i])) {
            scdk_panic("pmm allocated invalid page 0x%llx",
                       (unsigned long long)pages[i]);
        }

        for (uint32_t j = 0; j < i; j++) {
            if (pages[j] == pages[i]) {
                scdk_panic("pmm allocated duplicate page 0x%llx",
                           (unsigned long long)pages[i]);
            }
        }
    }
    scdk_log_write("test", "pmm allocated 4 KiB pages pass");

    scdk_pmm_get_stats(&after_alloc);
    if (after_alloc.free_pages + 3u != before.free_pages ||
        after_alloc.allocated_pages != before.allocated_pages + 3u) {
        scdk_panic("pmm allocation counters mismatch");
    }
    scdk_log_write("test", "pmm allocation counters pass");

    for (uint32_t i = 0; i < 3u; i++) {
        status = scdk_page_free(pages[i]);
        if (status != SCDK_OK) {
            scdk_panic("pmm free %u failed: %lld", i, (long long)status);
        }
    }

    scdk_pmm_get_stats(&after_free);
    if (after_free.free_pages != before.free_pages ||
        after_free.allocated_pages != before.allocated_pages) {
        scdk_panic("pmm free counters mismatch");
    }
    scdk_log_write("test", "pmm freed 4 KiB pages pass");

    status = scdk_page_free(pages[0]);
    require_status("pmm double-free rejected", status, SCDK_ERR_BUSY);

    scdk_log_write("test", "pmm: pass");
    scdk_log_write("boot", "pmm initialized");
}

static void run_vmm_selftest(void) {
    uint64_t cr3_phys = 0;
    uint64_t data_phys = 0;
    uint64_t translated = 0;
    uint64_t flags = 0;
    volatile uint64_t *mapped;
    volatile uint64_t *hhdm_view;
    scdk_status_t status;

    scdk_log_write("test", "vmm self-test start");

    if (selftest_hhdm_offset == 0u) {
        scdk_panic("vmm requires hhdm response");
    }

    status = scdk_vmm_init(selftest_hhdm_offset);
    require_status("vmm init", status, SCDK_OK);

    status = scdk_vmm_current_root(&cr3_phys);
    require_status("vmm current root", status, SCDK_OK);
    if ((cr3_phys & (SCDK_PAGE_SIZE - 1u)) != 0u || cr3_phys == 0u) {
        scdk_panic("vmm invalid cr3 0x%llx", (unsigned long long)cr3_phys);
    }
    scdk_log_write("test", "vmm cr3 inspect pass");

    status = scdk_page_alloc(&data_phys);
    require_status("vmm test page alloc", status, SCDK_OK);

    hhdm_view = (volatile uint64_t *)(uintptr_t)(selftest_hhdm_offset + data_phys);
    hhdm_view[0] = 0;
    hhdm_view[1] = 0;

    status = scdk_vmm_map_page(SCDK_VMM_SELFTEST_VIRT,
                               data_phys,
                               SCDK_VMM_MAP_WRITE);
    require_status("vmm map page", status, SCDK_OK);

    status = scdk_vmm_virt_to_phys(SCDK_VMM_SELFTEST_VIRT, &translated, &flags);
    require_status("vmm translate mapped page", status, SCDK_OK);
    if (translated != data_phys || (flags & SCDK_VMM_MAP_WRITE) == 0u) {
        scdk_panic("vmm translation mismatch");
    }
    scdk_log_write("test", "vmm page table translation pass");

    mapped = (volatile uint64_t *)(uintptr_t)SCDK_VMM_SELFTEST_VIRT;
    mapped[0] = 0x5343444b564d4d31ull;
    mapped[1] = 0x123456789abcdef0ull;
    if (mapped[0] != 0x5343444b564d4d31ull ||
        mapped[1] != 0x123456789abcdef0ull ||
        hhdm_view[0] != 0x5343444b564d4d31ull ||
        hhdm_view[1] != 0x123456789abcdef0ull) {
        scdk_panic("vmm mapped page read/write mismatch");
    }
    scdk_log_write("test", "vmm mapped page read/write pass");

    status = scdk_vmm_unmap_page(SCDK_VMM_SELFTEST_VIRT);
    require_status("vmm unmap page", status, SCDK_OK);

    status = scdk_vmm_virt_to_phys(SCDK_VMM_SELFTEST_VIRT, &translated, &flags);
    require_status("vmm unmapped lookup rejected", status, SCDK_ERR_NOENT);

    scdk_vmm_page_fault_placeholder(SCDK_VMM_SELFTEST_VIRT, 0);

    status = scdk_page_free(data_phys);
    require_status("vmm test page free", status, SCDK_OK);

    scdk_log_write("test", "vmm: pass");
    scdk_log_write("boot", "vmm initialized");
}

static void run_heap_selftest(void) {
    uint8_t *small;
    uint8_t *zeroed;
    uint8_t *reused;
    uint8_t *typed;
    scdk_status_t status;

    scdk_log_write("test", "heap self-test start");

    status = scdk_heap_init();
    require_status("heap init", status, SCDK_OK);

    small = scdk_kalloc(24u);
    if (small == 0) {
        scdk_panic("heap small allocation failed");
    }
    for (uint32_t i = 0; i < 24u; i++) {
        small[i] = (uint8_t)(0x30u + i);
    }

    zeroed = scdk_kzalloc(48u);
    if (zeroed == 0) {
        scdk_panic("heap zeroed allocation failed");
    }
    for (uint32_t i = 0; i < 48u; i++) {
        if (zeroed[i] != 0u) {
            scdk_panic("heap zeroed allocation contained stale data");
        }
    }

    scdk_kfree(small);
    scdk_kfree(zeroed);

    reused = scdk_kalloc(16u);
    if (reused == 0 || reused != small) {
        scdk_panic("heap free-list reuse failed");
    }
    scdk_log_write("heap", "alloc/free smoke test pass");

    typed = scdk_alloc_object_storage(SCDK_OBJ_SERVICE, 32u);
    if (typed == 0) {
        scdk_panic("heap typed object allocation failed");
    }
    for (uint32_t i = 0; i < 32u; i++) {
        if (typed[i] != 0u) {
            scdk_panic("heap typed object storage was not zeroed");
        }
    }
    scdk_log_write("heap", "typed allocation smoke test pass");

    if (scdk_kalloc((size_t)SCDK_HEAP_SIZE) != 0) {
        scdk_panic("heap impossible allocation unexpectedly succeeded");
    }
    scdk_log_write("test", "heap impossible allocation rejected pass");

    scdk_kfree(reused);
    scdk_kfree(typed);
    scdk_log_write("test", "heap: pass");
    scdk_log_write("boot", "heap initialized");
}

static void run_address_space_selftest(void) {
    scdk_cap_t aspace = 0;
    scdk_cap_t no_map_cap = 0;
    const struct scdk_cap_entry *entry = 0;
    uint64_t boot_root = 0;
    uint64_t page_phys = 0;
    uint64_t translated = 0;
    uint64_t flags = 0;
    volatile uint64_t *user_view;
    volatile uint64_t *hhdm_view;
    scdk_status_t status;

    scdk_log_write("test", "address-space self-test start");

    status = scdk_vmm_current_root(&boot_root);
    require_status("aspace boot root snapshot", status, SCDK_OK);

    status = scdk_address_space_create(&aspace);
    require_status("aspace create", status, SCDK_OK);
    scdk_log_write("aspace", "create pass");

    status = scdk_page_alloc(&page_phys);
    require_status("aspace test page alloc", status, SCDK_OK);

    hhdm_view = (volatile uint64_t *)(uintptr_t)(selftest_hhdm_offset + page_phys);
    hhdm_view[0] = 0;
    hhdm_view[1] = 0;

    status = scdk_address_space_map(aspace,
                                    SCDK_ASPACE_SELFTEST_VIRT,
                                    page_phys,
                                    SCDK_VMM_MAP_WRITE);
    require_status("aspace user map", status, SCDK_OK);
    scdk_log_write("aspace", "user map pass");

    status = scdk_address_space_translate(aspace,
                                          SCDK_ASPACE_SELFTEST_VIRT,
                                          &translated,
                                          &flags);
    require_status("aspace translate user page", status, SCDK_OK);
    if (translated != page_phys ||
        (flags & (SCDK_VMM_MAP_WRITE | SCDK_VMM_MAP_USER)) !=
            (SCDK_VMM_MAP_WRITE | SCDK_VMM_MAP_USER)) {
        scdk_panic("address-space translation mismatch");
    }

    status = scdk_address_space_activate(aspace);
    require_status("aspace activate", status, SCDK_OK);
    scdk_log_write("aspace", "activate pass");

    user_view = (volatile uint64_t *)(uintptr_t)SCDK_ASPACE_SELFTEST_VIRT;
    user_view[0] = 0x5343444b41535031ull;
    user_view[1] = 0x0fedcba987654321ull;
    if (user_view[0] != 0x5343444b41535031ull ||
        user_view[1] != 0x0fedcba987654321ull) {
        scdk_panic("address-space mapped read/write failed");
    }

    status = scdk_vmm_activate_root(boot_root);
    require_status("aspace restore boot root", status, SCDK_OK);
    if (hhdm_view[0] != 0x5343444b41535031ull ||
        hhdm_view[1] != 0x0fedcba987654321ull) {
        scdk_panic("address-space mapped data mismatch");
    }
    scdk_log_write("aspace", "mapped-page read/write pass");

    status = scdk_address_space_map(aspace,
                                    SCDK_KERNEL_VIRT_BASE,
                                    page_phys,
                                    SCDK_VMM_MAP_WRITE);
    require_status("aspace kernel range reject", status, SCDK_ERR_BOUNDS);
    scdk_log_write("aspace", "kernel range reject pass");

    status = scdk_cap_lookup(aspace, &entry);
    require_status("aspace cap lookup", status, SCDK_OK);

    status = scdk_cap_create(entry->object_id,
                             SCDK_RIGHT_READ | SCDK_RIGHT_WRITE,
                             &no_map_cap);
    require_status("aspace no-map cap create", status, SCDK_OK);

    status = scdk_address_space_map(no_map_cap,
                                    SCDK_ASPACE_SELFTEST_VIRT + SCDK_PAGE_SIZE,
                                    page_phys,
                                    SCDK_VMM_MAP_WRITE);
    require_status("aspace missing MAP right reject", status, SCDK_ERR_PERM);

    status = scdk_address_space_unmap(aspace, SCDK_ASPACE_SELFTEST_VIRT);
    require_status("aspace unmap", status, SCDK_OK);
    scdk_log_write("aspace", "unmap pass");

    status = scdk_address_space_translate(aspace,
                                          SCDK_ASPACE_SELFTEST_VIRT,
                                          &translated,
                                          &flags);
    require_status("aspace unmapped lookup rejected", status, SCDK_ERR_NOENT);

    status = scdk_page_free(page_phys);
    require_status("aspace test page free", status, SCDK_OK);

    scdk_log_write("test", "address-space: pass");
    scdk_log_write("boot", "address-space initialized");
}

struct scheduler_test_state {
    uint32_t step;
};

static void scheduler_test_thread_a(void *arg) {
    struct scheduler_test_state *state = arg;

    if (state == 0 || state->step != 0u) {
        scdk_panic("scheduler thread A step 1 order mismatch");
    }
    scdk_log_write("sched", "thread A step 1");
    state->step = 1u;
    scdk_yield();

    if (state->step != 2u) {
        scdk_panic("scheduler thread A step 2 order mismatch");
    }
    scdk_log_write("sched", "thread A step 2");
    state->step = 3u;
    scdk_yield();
}

static void scheduler_test_thread_b(void *arg) {
    struct scheduler_test_state *state = arg;

    if (state == 0 || state->step != 1u) {
        scdk_panic("scheduler thread B step 1 order mismatch");
    }
    scdk_log_write("sched", "thread B step 1");
    state->step = 2u;
    scdk_yield();

    if (state->step != 3u) {
        scdk_panic("scheduler thread B step 2 order mismatch");
    }
    scdk_log_write("sched", "thread B step 2");
    state->step = 4u;
    scdk_yield();
}

static void run_scheduler_selftest(void) {
    scdk_cap_t boot_task = 0;
    scdk_cap_t boot_thread = 0;
    scdk_cap_t thread_a = 0;
    scdk_cap_t thread_b = 0;
    scdk_cap_t current_task = 0;
    scdk_cap_t current_thread = 0;
    scdk_cap_t main_thread = 0;
    scdk_object_id_t owning_task = 0;
    uint32_t thread_state = SCDK_THREAD_NONE;
    const struct scdk_cap_entry *task_entry = 0;
    struct scheduler_test_state test_state = { 0 };
    scdk_status_t status;

    scdk_log_write("test", "scheduler self-test start");

    status = scdk_scheduler_init(&boot_task, &boot_thread);
    require_status("scheduler init", status, SCDK_OK);

    status = scdk_cap_check(boot_task, SCDK_RIGHT_READ, SCDK_OBJ_TASK, &task_entry);
    require_status("scheduler task cap", status, SCDK_OK);

    status = scdk_cap_check(boot_thread, SCDK_RIGHT_READ, SCDK_OBJ_THREAD, 0);
    require_status("scheduler thread cap", status, SCDK_OK);

    status = scdk_scheduler_current_task(&current_task);
    require_status("scheduler current task", status, SCDK_OK);
    if (current_task != boot_task) {
        scdk_panic("scheduler current task mismatch");
    }

    status = scdk_scheduler_current_thread(&current_thread);
    require_status("scheduler current thread", status, SCDK_OK);
    if (current_thread != boot_thread) {
        scdk_panic("scheduler current thread mismatch");
    }

    status = scdk_task_main_thread(boot_task, &main_thread);
    require_status("scheduler task main thread", status, SCDK_OK);
    if (main_thread != boot_thread) {
        scdk_panic("scheduler main thread mismatch");
    }

    status = scdk_thread_task_id(boot_thread, &owning_task);
    require_status("scheduler thread task owner", status, SCDK_OK);
    if (owning_task != task_entry->object_id) {
        scdk_panic("scheduler thread owner mismatch");
    }

    status = scdk_thread_state(boot_thread, &thread_state);
    require_status("scheduler thread state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_RUNNING) {
        scdk_panic("scheduler boot thread not running");
    }

    status = scdk_scheduler_yield_stub();
    require_status("scheduler yield stub", status, SCDK_OK);

    current_task = 0;
    current_thread = 0;
    status = scdk_scheduler_current_task(&current_task);
    require_status("scheduler current task after yield", status, SCDK_OK);
    status = scdk_scheduler_current_thread(&current_thread);
    require_status("scheduler current thread after yield", status, SCDK_OK);
    if (current_task != boot_task || current_thread != boot_thread) {
        scdk_panic("scheduler yield changed current context");
    }
    scdk_log_write("test", "scheduler yield preserves context pass");

    status = scdk_thread_create(boot_task,
                                scheduler_test_thread_a,
                                &test_state,
                                &thread_a);
    require_status("scheduler thread A create", status, SCDK_OK);

    status = scdk_thread_create(boot_task,
                                scheduler_test_thread_b,
                                &test_state,
                                &thread_b);
    require_status("scheduler thread B create", status, SCDK_OK);

    status = scdk_thread_state(thread_a, &thread_state);
    require_status("scheduler thread A new state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_NEW) {
        scdk_panic("scheduler thread A not NEW");
    }

    status = scdk_thread_start(thread_a);
    require_status("scheduler thread A start", status, SCDK_OK);
    status = scdk_thread_start(thread_b);
    require_status("scheduler thread B start", status, SCDK_OK);

    scdk_scheduler_run();
    if (test_state.step != 4u) {
        scdk_panic("scheduler cooperative interleave incomplete");
    }

    status = scdk_thread_state(thread_a, &thread_state);
    require_status("scheduler thread A dead state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_DEAD) {
        scdk_panic("scheduler thread A not DEAD");
    }

    status = scdk_thread_state(thread_b, &thread_state);
    require_status("scheduler thread B dead state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_DEAD) {
        scdk_panic("scheduler thread B not DEAD");
    }

    current_task = 0;
    current_thread = 0;
    status = scdk_scheduler_current_task(&current_task);
    require_status("scheduler current task after run", status, SCDK_OK);
    status = scdk_scheduler_current_thread(&current_thread);
    require_status("scheduler current thread after run", status, SCDK_OK);
    if (current_task != boot_task || current_thread != boot_thread) {
        scdk_panic("scheduler run did not restore boot context");
    }
    scdk_log_write("sched", "cooperative run queue pass");

    scdk_log_write("test", "scheduler: pass");
    scdk_log_write("boot", "scheduler initialized");
}

static void run_usermode_selftest(void) {
    scdk_status_t status;

    scdk_log_write("test", "user-mode self-test start");

    status = scdk_usermode_run_builtin_test(selftest_hhdm_offset);
    require_status("user-mode built-in stub", status, SCDK_OK);

    scdk_log_write("test", "user-mode: pass");
    scdk_log_write("boot", "user-mode initialized");
}

static void run_user_task_lifecycle_selftest(void) {
    scdk_cap_t task = 0;
    scdk_cap_t aspace = 0;
    scdk_cap_t main_thread = 0;
    uint32_t task_state = SCDK_TASK_NONE;
    uint32_t thread_state = SCDK_THREAD_NONE;
    scdk_status_t status;

    scdk_log_write("test", "user task lifecycle self-test start");

    status = scdk_user_task_create(&task, &aspace, &main_thread);
    require_status("user task create", status, SCDK_OK);

    if (task == 0 || aspace == 0 || main_thread == 0) {
        scdk_panic("user task lifecycle returned empty cap");
    }

    status = scdk_user_task_state(task, &task_state);
    require_status("user task new state", status, SCDK_OK);
    if (task_state != SCDK_TASK_NEW) {
        scdk_panic("user task not NEW");
    }

    status = scdk_user_thread_state(main_thread, &thread_state);
    require_status("user main thread new state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_NEW) {
        scdk_panic("user main thread not NEW");
    }

    status = scdk_user_task_run_builtin(task, selftest_hhdm_offset);
    require_status("user task run built-in stub", status, SCDK_OK);

    status = scdk_user_task_state(task, &task_state);
    require_status("user task dead state", status, SCDK_OK);
    if (task_state != SCDK_TASK_DEAD) {
        scdk_panic("user task not DEAD");
    }

    status = scdk_user_thread_state(main_thread, &thread_state);
    require_status("user main thread dead state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_DEAD) {
        scdk_panic("user main thread not DEAD");
    }

    status = scdk_task_cleanup(task);
    require_status("user task cleanup", status, SCDK_OK);

    status = scdk_user_task_state(task, &task_state);
    require_status("user task cleaned state", status, SCDK_OK);
    if (task_state != SCDK_TASK_CLEANED) {
        scdk_panic("user task not CLEANED");
    }

    scdk_log_write("test", "user task lifecycle: pass");
}

scdk_status_t scdk_run_core_selftests(void) {
    if (selftest_memmap == 0 || selftest_hhdm_offset == 0u) {
        return SCDK_ERR_INVAL;
    }

    run_object_capability_selftest();
    run_endpoint_message_selftest();
    run_ring_selftest();
    run_grant_selftest();
    run_pmm_selftest();
    run_vmm_selftest();
    run_heap_selftest();
    run_address_space_selftest();
    run_scheduler_selftest();
    run_usermode_selftest();
    run_user_task_lifecycle_selftest();

    scdk_log_write("test", "all core tests passed");
    return SCDK_OK;
}
