// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/selftest.h>

#include <stdint.h>

#include <scdk/address_space.h>
#include <scdk/capability.h>
#include <scdk/console.h>
#include <scdk/devmgr.h>
#include <scdk/endpoint.h>
#include <scdk/fault.h>
#include <scdk/grant.h>
#include <scdk/heap.h>
#include <scdk/initrd.h>
#include <scdk/keyboard.h>
#include <scdk/loader.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/mm.h>
#include <scdk/object.h>
#include <scdk/panic.h>
#include <scdk/proc_service.h>
#include <scdk/revoke.h>
#include <scdk/ring.h>
#include <scdk/scheduler.h>
#include <scdk/service.h>
#include <scdk/session.h>
#include <scdk/string.h>
#include <scdk/task.h>
#include <scdk/thread.h>
#include <scdk/timer.h>
#include <scdk/tmpfs.h>
#include <scdk/tty.h>
#include <scdk/user_grant.h>
#include <scdk/user_ipc.h>
#include <scdk/usermode.h>
#include <scdk/vfs.h>

#define SCDK_VMM_SELFTEST_VIRT 0xffffffffc0000000ull
#define SCDK_ASPACE_SELFTEST_VIRT 0x0000000000400000ull
#define SCDK_REVOKE_USER_GRANT_VIRT 0x0000000000600000ull
#define SCDK_CONSOLE_GRANT_SELFTEST_VIRT 0x0000000000700000ull
#define SCDK_USER_GRANT_TEST_PAYLOAD "grant payload"
#define SCDK_CONSOLE_GRANT_TEST_PAYLOAD "console grant path"

static const struct limine_memmap_response *selftest_memmap;
static uint64_t selftest_hhdm_offset;
static volatile bool preempt_thread_a_started;
static volatile bool preempt_thread_b_started;
static volatile bool preempt_thread_a_done;
static volatile bool preempt_thread_a_observed;
static volatile bool preempt_thread_b_observed;
static volatile uint64_t preempt_thread_a_tick;
static volatile uint64_t preempt_thread_b_tick;

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
    scdk_cap_t tty_endpoint = 0;
    scdk_cap_t looked_up_tty = 0;
    scdk_cap_t session_endpoint = 0;
    scdk_cap_t looked_up_session = 0;
    struct scdk_message msg;
    scdk_status_t status;

    scdk_log_write("test", "endpoint/message self-test start");

    status = scdk_console_service_init(&console_endpoint);
    require_status("console service init", status, SCDK_OK);

    status = scdk_log_set_console_endpoint(console_endpoint);
    require_status("console log route", status, SCDK_OK);

    status = scdk_console_backend_ready();
    require_status("console backend ready", status, SCDK_OK);
    scdk_log_write("console", "backend ready");

    status = scdk_console_direct_access_audit();
    require_status("console direct hardware access audit", status, SCDK_OK);
    scdk_log_write("console", "direct hardware access audit pass");

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

    scdk_message_init(&msg, 0, SCDK_SERVICE_CONSOLE, SCDK_MSG_CONSOLE_GET_INFO);
    status = scdk_endpoint_call(looked_up_endpoint, &msg);
    require_status("console get info", status, SCDK_OK);
    if (msg.arg0 == 0u || msg.arg1 == 0u) {
        scdk_panic("console info returned empty geometry");
    }
    scdk_log_write("test", "console info path pass");

    scdk_message_init(&msg, 0, SCDK_SERVICE_CONSOLE, SCDK_MSG_CONSOLE_CLEAR);
    status = scdk_endpoint_call(looked_up_endpoint, &msg);
    require_status("console clear", status, SCDK_OK);
    scdk_log_write("test", "console clear path pass");

    status = scdk_tty_service_init(&tty_endpoint);
    require_status("tty service init", status, SCDK_OK);

    status = scdk_service_lookup(SCDK_SERVICE_TTY, &looked_up_tty);
    require_status("tty service lookup", status, SCDK_OK);
    if (looked_up_tty != tty_endpoint) {
        scdk_panic("tty service endpoint mismatch");
    }

    status = scdk_keyboard_inject_test_key('a');
    require_status("tty inject key", status, SCDK_OK);

    scdk_message_init(&msg, 0, SCDK_SERVICE_TTY, SCDK_MSG_TTY_POLL_EVENT);
    status = scdk_endpoint_call(looked_up_tty, &msg);
    require_status("tty poll event", status, SCDK_OK);
    if ((uint32_t)msg.arg2 != 'a' ||
        (uint32_t)(msg.arg1 >> 32u) != SCDK_INPUT_KEY_DOWN) {
        scdk_panic("tty event payload mismatch");
    }
    scdk_log_write("tty", "input event path pass");

    status = scdk_session_service_init(&session_endpoint);
    require_status("session service init", status, SCDK_OK);

    status = scdk_service_lookup(SCDK_SERVICE_SESSION, &looked_up_session);
    require_status("session service lookup", status, SCDK_OK);
    if (looked_up_session != session_endpoint) {
        scdk_panic("session service endpoint mismatch");
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_SESSION, SCDK_MSG_SERVICE_LOOKUP);
    msg.arg0 = SCDK_SERVICE_CONSOLE;
    status = scdk_endpoint_call(looked_up_session, &msg);
    require_status("session lookup console", status, SCDK_OK);
    if ((scdk_cap_t)msg.arg0 != console_endpoint) {
        scdk_panic("session console cap mismatch");
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_SESSION, SCDK_MSG_SERVICE_LOOKUP);
    msg.arg0 = SCDK_SERVICE_TTY;
    status = scdk_endpoint_call(looked_up_session, &msg);
    require_status("session lookup tty", status, SCDK_OK);
    if ((scdk_cap_t)msg.arg0 != tty_endpoint) {
        scdk_panic("session tty cap mismatch");
    }
    scdk_log_write("m31", "session lookup path pass");

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

static void run_initrd_selftest(void) {
    struct scdk_initrd_file file;
    scdk_status_t status;

    scdk_log_write("test", "initrd self-test start");

    status = scdk_initrd_init_from_limine();
    require_status("initrd init", status, SCDK_OK);

    status = scdk_initrd_list();
    require_status("initrd list", status, SCDK_OK);

    status = scdk_initrd_find("/init", &file);
    require_status("initrd find /init", status, SCDK_OK);
    if (file.data == 0 || file.size == 0u) {
        scdk_panic("initrd /init has no data");
    }

    status = scdk_initrd_find("/etc/scdk.conf", &file);
    require_status("initrd find /etc/scdk.conf", status, SCDK_OK);
    if (file.data == 0 || file.size == 0u) {
        scdk_panic("initrd /etc/scdk.conf has no data");
    }

    status = scdk_initrd_find("/hello.txt", &file);
    require_status("initrd find /hello.txt", status, SCDK_OK);
    if (file.data == 0 || file.size == 0u) {
        scdk_panic("initrd /hello.txt has no data");
    }

    status = scdk_initrd_find("/hello", &file);
    require_status("initrd find /hello", status, SCDK_OK);
    if (file.data == 0 || file.size == 0u) {
        scdk_panic("initrd /hello has no data");
    }

    status = scdk_initrd_find("/grant-test", &file);
    require_status("initrd find /grant-test", status, SCDK_OK);
    if (file.data == 0 || file.size == 0u) {
        scdk_panic("initrd /grant-test has no data");
    }

    status = scdk_initrd_find("/ring-test", &file);
    require_status("initrd find /ring-test", status, SCDK_OK);
    if (file.data == 0 || file.size == 0u) {
        scdk_panic("initrd /ring-test has no data");
    }

    scdk_log_write("test", "initrd: pass");
}

static scdk_status_t tmpfs_open_path(scdk_cap_t endpoint,
                                     const char *path,
                                     scdk_cap_t *out_file) {
    struct scdk_message msg;
    scdk_status_t status;

    if (path == 0 || out_file == 0) {
        return SCDK_ERR_INVAL;
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_TMPFS, SCDK_MSG_OPEN);
    msg.arg0 = (uint64_t)(uintptr_t)path;
    msg.arg1 = 0;

    status = scdk_endpoint_call(endpoint, &msg);
    if (status != SCDK_OK) {
        return status;
    }

    *out_file = (scdk_cap_t)msg.arg0;
    return msg.arg0 == 0u ? SCDK_ERR_NOENT : SCDK_OK;
}

static scdk_status_t tmpfs_read_file(scdk_cap_t endpoint,
                                     scdk_cap_t file,
                                     char *buffer,
                                     uint64_t buffer_size,
                                     uint64_t *out_read) {
    struct scdk_message msg;
    scdk_status_t status;

    if (buffer == 0 || buffer_size == 0u || out_read == 0) {
        return SCDK_ERR_INVAL;
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_TMPFS, SCDK_MSG_READ);
    msg.arg0 = file;
    msg.arg1 = 0;
    msg.arg2 = (uint64_t)(uintptr_t)buffer;
    msg.arg3 = buffer_size - 1u;

    status = scdk_endpoint_call(endpoint, &msg);
    if (status != SCDK_OK) {
        return status;
    }

    *out_read = msg.arg0;
    if (*out_read >= buffer_size) {
        return SCDK_ERR_BOUNDS;
    }

    buffer[*out_read] = '\0';
    return SCDK_OK;
}

static scdk_status_t tmpfs_close_file(scdk_cap_t endpoint,
                                      scdk_cap_t file) {
    struct scdk_message msg;

    scdk_message_init(&msg, 0, SCDK_SERVICE_TMPFS, SCDK_MSG_CLOSE);
    msg.arg0 = file;
    return scdk_endpoint_call(endpoint, &msg);
}

static void run_tmpfs_selftest(void) {
    scdk_cap_t endpoint = 0;
    scdk_cap_t looked_up = 0;
    scdk_cap_t hello_file = 0;
    scdk_cap_t init_file = 0;
    char buffer[128];
    uint64_t bytes_read = 0;
    scdk_status_t status;

    scdk_log_write("test", "tmpfs self-test start");

    status = scdk_tmpfs_service_init(&endpoint);
    require_status("tmpfs service init", status, SCDK_OK);

    status = scdk_service_lookup(SCDK_SERVICE_TMPFS, &looked_up);
    require_status("tmpfs service lookup", status, SCDK_OK);
    if (looked_up != endpoint) {
        scdk_panic("tmpfs service endpoint mismatch");
    }

    status = tmpfs_open_path(looked_up, "/hello.txt", &hello_file);
    require_status("tmpfs open /hello.txt", status, SCDK_OK);
    scdk_log_write("tmpfs", "open /hello.txt pass");

    status = tmpfs_read_file(looked_up,
                             hello_file,
                             buffer,
                             sizeof(buffer),
                             &bytes_read);
    require_status("tmpfs read /hello.txt", status, SCDK_OK);
    if (bytes_read < 5u || memcmp(buffer, "hello", 5u) != 0) {
        scdk_panic("tmpfs /hello.txt content mismatch");
    }
    scdk_log_write("tmpfs", "read /hello.txt pass");

    status = tmpfs_open_path(looked_up, "/init", &init_file);
    require_status("tmpfs open /init", status, SCDK_OK);

    status = tmpfs_read_file(looked_up,
                             init_file,
                             buffer,
                             sizeof(buffer),
                             &bytes_read);
    require_status("tmpfs read /init", status, SCDK_OK);
    if (bytes_read == 0u) {
        scdk_panic("tmpfs /init read no data");
    }
    scdk_log_write("test", "tmpfs read /init pass");

    struct scdk_message write_msg;
    scdk_message_init(&write_msg, 0, SCDK_SERVICE_TMPFS, SCDK_MSG_WRITE);
    write_msg.arg0 = hello_file;
    write_msg.arg1 = 0;
    write_msg.arg2 = (uint64_t)(uintptr_t)"ignored";
    write_msg.arg3 = 7u;
    status = scdk_endpoint_call(looked_up, &write_msg);
    require_status("tmpfs write rejected", status, SCDK_ERR_NOTSUP);

    status = tmpfs_close_file(looked_up, hello_file);
    require_status("tmpfs close /hello.txt", status, SCDK_OK);

    status = tmpfs_close_file(looked_up, init_file);
    require_status("tmpfs close /init", status, SCDK_OK);

    scdk_log_write("test", "tmpfs: pass");
}

static scdk_status_t vfs_open_path(scdk_cap_t endpoint,
                                   const char *path,
                                   scdk_cap_t *out_file) {
    struct scdk_message msg;
    scdk_status_t status;

    if (path == 0 || out_file == 0) {
        return SCDK_ERR_INVAL;
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_OPEN);
    msg.arg0 = (uint64_t)(uintptr_t)path;
    msg.arg1 = 0;

    status = scdk_endpoint_call(endpoint, &msg);
    if (status != SCDK_OK) {
        return status;
    }

    *out_file = (scdk_cap_t)msg.arg0;
    return msg.arg0 == 0u ? SCDK_ERR_NOENT : SCDK_OK;
}

static scdk_status_t vfs_read_file(scdk_cap_t endpoint,
                                   scdk_cap_t file,
                                   char *buffer,
                                   uint64_t buffer_size,
                                   uint64_t *out_read) {
    struct scdk_message msg;
    scdk_status_t status;

    if (buffer == 0 || buffer_size == 0u || out_read == 0) {
        return SCDK_ERR_INVAL;
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_READ);
    msg.arg0 = file;
    msg.arg1 = 0;
    msg.arg2 = (uint64_t)(uintptr_t)buffer;
    msg.arg3 = buffer_size - 1u;

    status = scdk_endpoint_call(endpoint, &msg);
    if (status != SCDK_OK) {
        return status;
    }

    *out_read = msg.arg0;
    if (*out_read >= buffer_size) {
        return SCDK_ERR_BOUNDS;
    }

    buffer[*out_read] = '\0';
    return SCDK_OK;
}

static scdk_status_t vfs_close_file(scdk_cap_t endpoint,
                                    scdk_cap_t file) {
    struct scdk_message msg;

    scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_CLOSE);
    msg.arg0 = file;
    return scdk_endpoint_call(endpoint, &msg);
}

static void run_vfs_selftest(void) {
    scdk_cap_t endpoint = 0;
    scdk_cap_t looked_up = 0;
    scdk_cap_t hello_file = 0;
    char buffer[128];
    uint64_t bytes_read = 0;
    scdk_status_t status;

    scdk_log_write("test", "vfs self-test start");

    status = scdk_vfs_service_init(&endpoint);
    require_status("vfs service init", status, SCDK_OK);

    status = scdk_service_lookup(SCDK_SERVICE_VFS, &looked_up);
    require_status("vfs service lookup", status, SCDK_OK);
    if (looked_up != endpoint) {
        scdk_panic("vfs service endpoint mismatch");
    }

    status = vfs_open_path(looked_up, "/hello.txt", &hello_file);
    require_status("vfs open /hello.txt", status, SCDK_OK);
    scdk_log_write("vfs", "open /hello.txt pass");

    status = vfs_read_file(looked_up,
                           hello_file,
                           buffer,
                           sizeof(buffer),
                           &bytes_read);
    require_status("vfs read /hello.txt", status, SCDK_OK);
    if (bytes_read < 5u || memcmp(buffer, "hello", 5u) != 0) {
        scdk_panic("vfs /hello.txt content mismatch");
    }
    scdk_log_write("vfs", "read /hello.txt pass");

    status = vfs_close_file(looked_up, hello_file);
    require_status("vfs close /hello.txt", status, SCDK_OK);

    scdk_log_write("test", "vfs: pass");
}

static bool dirents_contain(const struct scdk_vfs_dirent *entries,
                            uint64_t count,
                            const char *name,
                            uint64_t type) {
    for (uint64_t i = 0; i < count; i++) {
        if (entries[i].type == type &&
            strlen(entries[i].name) == strlen(name) &&
            memcmp(entries[i].name, name, strlen(name)) == 0) {
            return true;
        }
    }

    return false;
}

static void run_m31_vfs_session_selftest(void) {
    scdk_cap_t session = 0;
    scdk_cap_t vfs = 0;
    scdk_cap_t proc = 0;
    struct scdk_vfs_dirent entries[16];
    struct scdk_message msg;
    scdk_status_t status;

    scdk_log_write("test", "m31 vfs/session self-test start");

    status = scdk_service_lookup(SCDK_SERVICE_SESSION, &session);
    require_status("m31 session service lookup", status, SCDK_OK);

    scdk_message_init(&msg, 0, SCDK_SERVICE_SESSION, SCDK_MSG_SERVICE_LOOKUP);
    msg.arg0 = SCDK_SERVICE_VFS;
    status = scdk_endpoint_call(session, &msg);
    require_status("m31 session lookup vfs", status, SCDK_OK);
    vfs = (scdk_cap_t)msg.arg0;

    scdk_message_init(&msg, 0, SCDK_SERVICE_SESSION, SCDK_MSG_SERVICE_LOOKUP);
    msg.arg0 = SCDK_SERVICE_PROC;
    status = scdk_endpoint_call(session, &msg);
    require_status("m31 session lookup proc", status, SCDK_OK);
    proc = (scdk_cap_t)msg.arg0;
    if (vfs == 0u || proc == 0u) {
        scdk_panic("m31 session returned empty service cap");
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_STAT);
    msg.arg0 = (uint64_t)(uintptr_t)"/";
    msg.arg1 = 1;
    status = scdk_endpoint_call(vfs, &msg);
    require_status("m31 vfs stat /", status, SCDK_OK);
    if (msg.arg0 != SCDK_VFS_NODE_DIR) {
        scdk_panic("m31 vfs root is not a directory");
    }

    memset(entries, 0, sizeof(entries));
    scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_LISTDIR);
    msg.arg0 = (uint64_t)(uintptr_t)"/";
    msg.arg1 = 1;
    msg.arg2 = (uint64_t)(uintptr_t)entries;
    msg.arg3 = sizeof(entries);
    status = scdk_endpoint_call(vfs, &msg);
    require_status("m31 vfs listdir /", status, SCDK_OK);
    if (!dirents_contain(entries, msg.arg0, "bin", SCDK_VFS_NODE_DIR) ||
        !dirents_contain(entries, msg.arg0, "hello", SCDK_VFS_NODE_FILE)) {
        scdk_panic("m31 root directory entries missing");
    }

    memset(entries, 0, sizeof(entries));
    scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_LISTDIR);
    msg.arg0 = (uint64_t)(uintptr_t)"/bin";
    msg.arg1 = 4;
    msg.arg2 = (uint64_t)(uintptr_t)entries;
    msg.arg3 = sizeof(entries);
    status = scdk_endpoint_call(vfs, &msg);
    require_status("m31 vfs listdir /bin", status, SCDK_OK);
    if (!dirents_contain(entries, msg.arg0, "hello", SCDK_VFS_NODE_FILE)) {
        scdk_panic("m31 /bin/hello directory entry missing");
    }

    scdk_log_write("m31", "vfs stat/listdir path pass");
}

static void run_loader_selftest(void) {
    scdk_cap_t task = 0;
    scdk_cap_t main_thread = 0;
    uint32_t task_state = SCDK_TASK_NONE;
    uint32_t thread_state = SCDK_THREAD_NONE;
    scdk_status_t status;

    scdk_log_write("test", "loader self-test start");

    status = scdk_loader_load_from_vfs("/hello",
                                       selftest_hhdm_offset,
                                       &task,
                                       &main_thread);
    require_status("loader load /hello", status, SCDK_OK);

    status = scdk_user_task_state(task, &task_state);
    require_status("loader task dead state", status, SCDK_OK);
    if (task_state != SCDK_TASK_DEAD) {
        scdk_panic("loader task did not exit");
    }

    status = scdk_user_thread_state(main_thread, &thread_state);
    require_status("loader main thread dead state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_DEAD) {
        scdk_panic("loader main thread did not exit");
    }

    status = scdk_task_cleanup(task);
    require_status("loader task cleanup", status, SCDK_OK);

    scdk_log_write("test", "loader: pass");
}

static void run_proc_selftest(void) {
    scdk_cap_t endpoint = 0;
    scdk_cap_t looked_up = 0;
    struct scdk_message msg;
    scdk_status_t status;

    scdk_log_write("test", "proc self-test start");

    status = scdk_proc_service_init(selftest_hhdm_offset, &endpoint);
    require_status("proc service init", status, SCDK_OK);

    status = scdk_service_lookup(SCDK_SERVICE_PROC, &looked_up);
    require_status("proc service lookup", status, SCDK_OK);
    if (looked_up != endpoint) {
        scdk_panic("proc service endpoint mismatch");
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_PROC, SCDK_MSG_PROCESS_SPAWN);
    msg.arg0 = (uint64_t)(uintptr_t)"/hello";
    msg.arg1 = 6;
    msg.arg2 = 0;
    status = scdk_endpoint_call(looked_up, &msg);
    require_status("proc spawn /hello", status, SCDK_OK);
    if (msg.arg0 == 0u) {
        scdk_panic("proc spawn returned empty task cap");
    }

    scdk_log_write("test", "proc: pass");
}

static scdk_status_t grant_test_endpoint_handler(scdk_cap_t endpoint,
                                                 struct scdk_message *msg,
                                                 void *context) {
    char buffer[32];
    scdk_status_t status;

    (void)context;

    if (msg == 0 || msg->type != SCDK_MSG_READ) {
        return SCDK_ERR_INVAL;
    }

    if (msg->arg3 == 1u) {
        status = scdk_user_grant_copy_from(endpoint,
                                           (scdk_cap_t)msg->arg0,
                                           msg->arg1,
                                           buffer,
                                           msg->arg2);
        if (status != SCDK_OK) {
            return status;
        }

        if (msg->arg2 != (uint64_t)(sizeof(SCDK_USER_GRANT_TEST_PAYLOAD) - 1u) ||
            memcmp(buffer, SCDK_USER_GRANT_TEST_PAYLOAD, sizeof(SCDK_USER_GRANT_TEST_PAYLOAD) - 1u) != 0) {
            return SCDK_ERR_INVAL;
        }
        scdk_log_write("grant", "user read grant pass");

        status = scdk_validate_grant_access((scdk_cap_t)msg->arg0,
                                            0,
                                            1,
                                            SCDK_RIGHT_WRITE);
        if (status == SCDK_OK) {
            return SCDK_ERR_PERM;
        }
        scdk_log_write("grant", "write denied pass");

        status = scdk_validate_grant_access((scdk_cap_t)msg->arg0,
                                            msg->arg2,
                                            1,
                                            SCDK_RIGHT_READ);
        if (status == SCDK_OK) {
            return SCDK_ERR_BOUNDS;
        }
        scdk_log_write("grant", "bounds reject pass");
        return SCDK_OK;
    }

    if (msg->arg3 == 2u) {
        status = scdk_validate_grant_access((scdk_cap_t)msg->arg0,
                                            0,
                                            1,
                                            SCDK_RIGHT_READ);
        if (status == SCDK_OK) {
            return SCDK_ERR_PERM;
        }
        scdk_log_write("grant", "revoke pass");
        return SCDK_OK;
    }

    return SCDK_ERR_INVAL;
}

static void run_user_grant_selftest(void) {
    scdk_cap_t endpoint = 0;
    scdk_cap_t looked_up = 0;
    scdk_cap_t task = 0;
    scdk_cap_t main_thread = 0;
    uint32_t task_state = SCDK_TASK_NONE;
    uint32_t thread_state = SCDK_THREAD_NONE;
    scdk_status_t status;

    scdk_log_write("test", "user grant self-test start");

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  grant_test_endpoint_handler,
                                  0,
                                  &endpoint);
    require_status("grant test endpoint create", status, SCDK_OK);

    status = scdk_service_register(SCDK_SERVICE_GRANT_TEST, endpoint);
    require_status("grant test service register", status, SCDK_OK);

    status = scdk_service_lookup(SCDK_SERVICE_GRANT_TEST, &looked_up);
    require_status("grant test service lookup", status, SCDK_OK);
    if (looked_up != endpoint) {
        scdk_panic("grant test service endpoint mismatch");
    }

    status = scdk_loader_load_from_vfs_with_endpoint("/grant-test",
                                                     selftest_hhdm_offset,
                                                     looked_up,
                                                     &task,
                                                     &main_thread);
    require_status("user grant task run", status, SCDK_OK);

    status = scdk_user_task_state(task, &task_state);
    require_status("user grant task dead state", status, SCDK_OK);
    if (task_state != SCDK_TASK_DEAD) {
        scdk_panic("user grant task did not exit");
    }

    status = scdk_user_thread_state(main_thread, &thread_state);
    require_status("user grant thread dead state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_DEAD) {
        scdk_panic("user grant thread did not exit");
    }

    status = scdk_task_cleanup(task);
    require_status("user grant task cleanup", status, SCDK_OK);

    scdk_log_write("test", "user grant: pass");
}

static void run_console_grant_write_selftest(void) {
    scdk_cap_t console_endpoint = 0;
    scdk_cap_t source_task = 0;
    scdk_cap_t grant = 0;
    uint64_t phys = 0;
    volatile char *payload;
    struct scdk_message msg;
    scdk_status_t status;

    scdk_log_write("test", "console grant-write self-test start");

    status = scdk_service_lookup(SCDK_SERVICE_CONSOLE, &console_endpoint);
    require_status("console grant service lookup", status, SCDK_OK);

    status = scdk_page_alloc(&phys);
    require_status("console grant page alloc", status, SCDK_OK);

    status = scdk_vmm_map_page(SCDK_CONSOLE_GRANT_SELFTEST_VIRT,
                               phys,
                               SCDK_VMM_MAP_USER | SCDK_VMM_MAP_WRITE);
    require_status("console grant page map", status, SCDK_OK);

    payload = (volatile char *)(uintptr_t)SCDK_CONSOLE_GRANT_SELFTEST_VIRT;
    for (uint32_t i = 0; i < sizeof(SCDK_CONSOLE_GRANT_TEST_PAYLOAD) - 1u; i++) {
        payload[i] = SCDK_CONSOLE_GRANT_TEST_PAYLOAD[i];
    }

    status = scdk_task_create(SCDK_BOOT_CORE, 0, &source_task);
    require_status("console grant source task create", status, SCDK_OK);

    status = scdk_user_grant_create(source_task,
                                    SCDK_CONSOLE_GRANT_SELFTEST_VIRT,
                                    sizeof(SCDK_CONSOLE_GRANT_TEST_PAYLOAD) - 1u,
                                    SCDK_RIGHT_READ,
                                    console_endpoint,
                                    &grant);
    require_status("console grant create", status, SCDK_OK);

    scdk_message_init(&msg, 0, SCDK_SERVICE_CONSOLE, SCDK_MSG_CONSOLE_WRITE);
    msg.arg0 = grant;
    msg.arg1 = 0;
    msg.arg2 = sizeof(SCDK_CONSOLE_GRANT_TEST_PAYLOAD) - 1u;
    msg.arg3 = 0;
    status = scdk_endpoint_call(console_endpoint, &msg);
    require_status("console grant write", status, SCDK_OK);
    scdk_log_write("console", "grant write path pass");

    status = scdk_user_grant_revoke(grant);
    require_status("console grant revoke", status, SCDK_OK);

    scdk_message_init(&msg, 0, SCDK_SERVICE_CONSOLE, SCDK_MSG_CONSOLE_WRITE);
    msg.arg0 = grant;
    msg.arg1 = 0;
    msg.arg2 = 1;
    status = scdk_endpoint_call(console_endpoint, &msg);
    require_status("console revoked grant write rejected", status, SCDK_ERR_PERM);

    status = scdk_vmm_unmap_page(SCDK_CONSOLE_GRANT_SELFTEST_VIRT);
    require_status("console grant page unmap", status, SCDK_OK);

    status = scdk_page_free(phys);
    require_status("console grant page free", status, SCDK_OK);

    scdk_log_write("test", "console grant-write: pass");
}

static void run_user_ring_selftest(void) {
    scdk_cap_t task = 0;
    scdk_cap_t main_thread = 0;
    uint32_t task_state = SCDK_TASK_NONE;
    uint32_t thread_state = SCDK_THREAD_NONE;
    scdk_status_t status;

    scdk_log_write("test", "user ring self-test start");

    status = scdk_loader_load_from_vfs("/ring-test",
                                       selftest_hhdm_offset,
                                       &task,
                                       &main_thread);
    require_status("user ring task run", status, SCDK_OK);

    status = scdk_user_task_state(task, &task_state);
    require_status("user ring task dead state", status, SCDK_OK);
    if (task_state != SCDK_TASK_DEAD) {
        scdk_panic("user ring task did not exit");
    }

    status = scdk_user_thread_state(main_thread, &thread_state);
    require_status("user ring thread dead state", status, SCDK_OK);
    if (thread_state != SCDK_THREAD_DEAD) {
        scdk_panic("user ring thread did not exit");
    }

    status = scdk_task_cleanup(task);
    require_status("user ring task cleanup", status, SCDK_OK);

    scdk_log_write("test", "user ring: pass");
}

static scdk_status_t revoke_test_endpoint_handler(scdk_cap_t endpoint,
                                                  struct scdk_message *msg,
                                                  void *context) {
    (void)endpoint;
    (void)msg;
    (void)context;
    return SCDK_OK;
}

static void run_revoke_selftest(void) {
    scdk_object_id_t object_id = 0;
    scdk_cap_t cap = 0;
    scdk_cap_t replacement = 0;
    scdk_cap_t endpoint = 0;
    scdk_cap_t grant_endpoint = 0;
    scdk_cap_t ring = 0;
    scdk_cap_t bound = 0;
    scdk_cap_t source_task = 0;
    scdk_cap_t user_grant = 0;
    uint64_t phys = 0;
    const struct scdk_cap_entry *entry = 0;
    scdk_status_t status;

    scdk_log_write("test", "revoke self-test start");

    status = scdk_object_create(SCDK_OBJ_SERVICE,
                                SCDK_BOOT_CORE,
                                0,
                                0,
                                &object_id);
    require_status("revoke object create", status, SCDK_OK);

    status = scdk_cap_create(object_id,
                             SCDK_RIGHT_READ | SCDK_RIGHT_WRITE,
                             &cap);
    require_status("revoke cap create", status, SCDK_OK);

    status = scdk_cap_check(cap, SCDK_RIGHT_READ, SCDK_OBJ_SERVICE, 0);
    require_status("revoke cap precheck", status, SCDK_OK);

    status = scdk_revoke_capability(cap);
    require_status("revoke cap", status, SCDK_OK);

    status = scdk_cap_lookup(cap, &entry);
    require_status("revoked cap lookup rejected", status, SCDK_ERR_NOENT);
    scdk_log_write("revoke", "cap revoke pass");

    status = scdk_cap_create(object_id, SCDK_RIGHT_READ, &replacement);
    require_status("revoke replacement cap create", status, SCDK_OK);
    if (replacement == cap) {
        scdk_panic("revoked capability token was reused without generation bump");
    }

    status = scdk_cap_check(cap, SCDK_RIGHT_READ, SCDK_OBJ_SERVICE, 0);
    require_status("stale cap generation rejected", status, SCDK_ERR_NOENT);
    scdk_log_write("revoke", "stale generation reject pass");

    status = scdk_cap_check(replacement, SCDK_RIGHT_READ, SCDK_OBJ_SERVICE, 0);
    require_status("replacement cap check", status, SCDK_OK);

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  revoke_test_endpoint_handler,
                                  0,
                                  &endpoint);
    require_status("revoke ring endpoint create", status, SCDK_OK);

    status = scdk_ring_create(SCDK_BOOT_CORE, 4u, 0, &ring);
    require_status("revoke ring create", status, SCDK_OK);

    status = scdk_ring_bind_target(ring, endpoint);
    require_status("revoke ring bind", status, SCDK_OK);

    status = scdk_revoke_capability(endpoint);
    require_status("revoke ring endpoint cap", status, SCDK_OK);

    status = scdk_ring_bound_target(ring, &bound);
    require_status("revoke ring bound target rejected", status, SCDK_ERR_NOENT);
    scdk_log_write("revoke", "ring binding reject pass");

    status = scdk_page_alloc(&phys);
    require_status("revoke user grant page alloc", status, SCDK_OK);

    status = scdk_vmm_map_page(SCDK_REVOKE_USER_GRANT_VIRT,
                               phys,
                               SCDK_VMM_MAP_USER | SCDK_VMM_MAP_WRITE);
    require_status("revoke user grant page map", status, SCDK_OK);

    status = scdk_task_create(SCDK_BOOT_CORE, 0, &source_task);
    require_status("revoke grant source task create", status, SCDK_OK);

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  revoke_test_endpoint_handler,
                                  0,
                                  &grant_endpoint);
    require_status("revoke grant endpoint create", status, SCDK_OK);

    status = scdk_user_grant_create(source_task,
                                    SCDK_REVOKE_USER_GRANT_VIRT,
                                    16u,
                                    SCDK_RIGHT_READ,
                                    grant_endpoint,
                                    &user_grant);
    require_status("revoke user grant create", status, SCDK_OK);

    status = scdk_validate_grant_access(user_grant,
                                        0,
                                        1,
                                        SCDK_RIGHT_READ);
    require_status("revoke user grant precheck", status, SCDK_OK);

    status = scdk_revoke_capability(grant_endpoint);
    require_status("revoke grant endpoint cap", status, SCDK_OK);

    status = scdk_validate_grant_access(user_grant,
                                        0,
                                        1,
                                        SCDK_RIGHT_READ);
    require_status("revoke grant binding rejected", status, SCDK_ERR_NOENT);
    scdk_log_write("revoke", "grant binding reject pass");

    status = scdk_vmm_unmap_page(SCDK_REVOKE_USER_GRANT_VIRT);
    require_status("revoke user grant page unmap", status, SCDK_OK);

    status = scdk_page_free(phys);
    require_status("revoke user grant page free", status, SCDK_OK);

    scdk_log_write("test", "revoke: pass");
}

static void run_one_fault_selftest(enum scdk_fault_user_test test,
                                   const char *name) {
    scdk_cap_t task = 0;
    scdk_cap_t aspace = 0;
    scdk_cap_t main_thread = 0;
    uint32_t task_state = SCDK_TASK_NONE;
    uint32_t thread_state = SCDK_THREAD_NONE;
    scdk_status_t status;

    scdk_fault_reset_test_state();

    status = scdk_user_task_create(&task, &aspace, &main_thread);
    require_status(name, status, SCDK_OK);

    status = scdk_user_task_run_fault_test(task, test, selftest_hhdm_offset);
    require_status(name, status, SCDK_OK);

    status = scdk_user_task_state(task, &task_state);
    require_status(name, status, SCDK_OK);
    if (task_state != SCDK_TASK_DEAD) {
        scdk_panic("%s did not kill the user task", name);
    }

    status = scdk_user_thread_state(main_thread, &thread_state);
    require_status(name, status, SCDK_OK);
    if (thread_state != SCDK_THREAD_DEAD) {
        scdk_panic("%s did not kill the user thread", name);
    }

    switch (test) {
    case SCDK_FAULT_TEST_PAGE_FAULT:
        if (!scdk_fault_saw_user_page_fault()) {
            scdk_panic("page fault self-test did not reach fault handler");
        }
        scdk_log_write("sched", "continuing");
        break;
    case SCDK_FAULT_TEST_INVALID_SYSCALL:
        if (!scdk_fault_saw_invalid_syscall()) {
            scdk_panic("invalid syscall self-test did not reach fault handler");
        }
        break;
    case SCDK_FAULT_TEST_BAD_POINTER:
        if (!scdk_fault_saw_bad_user_pointer()) {
            scdk_panic("bad pointer self-test did not reach fault handler");
        }
        break;
    default:
        scdk_panic("unknown fault self-test kind");
    }

    status = scdk_task_cleanup(task);
    require_status(name, status, SCDK_OK);
}

static void run_fault_selftest(void) {
    scdk_log_write("test", "fault self-test start");

    run_one_fault_selftest(SCDK_FAULT_TEST_PAGE_FAULT,
                           "fault user page fault");
    run_one_fault_selftest(SCDK_FAULT_TEST_INVALID_SYSCALL,
                           "fault invalid syscall");
    run_one_fault_selftest(SCDK_FAULT_TEST_BAD_POINTER,
                           "fault bad user pointer");

    scdk_log_write("test", "fault: pass");
}

static void preempt_pause(void) {
    __asm__ volatile ("pause" ::: "memory");
}

static void preempt_thread_a(void *arg) {
    (void)arg;

    scdk_timer_enable_preemption();
    preempt_thread_a_tick = scdk_timer_ticks();
    preempt_thread_a_started = true;

    while (!preempt_thread_b_started) {
        preempt_pause();
    }

    while (scdk_timer_ticks() == preempt_thread_a_tick) {
        preempt_pause();
    }

    preempt_thread_a_observed = true;
    preempt_thread_a_done = true;
}

static void preempt_thread_b(void *arg) {
    (void)arg;

    preempt_thread_b_tick = scdk_timer_ticks();
    preempt_thread_b_started = true;

    while (!preempt_thread_a_done) {
        preempt_pause();
    }

    while (scdk_timer_ticks() == preempt_thread_b_tick) {
        preempt_pause();
    }

    preempt_thread_b_observed = true;
}

static void run_timer_preemption_selftest(void) {
    scdk_cap_t task = 0;
    scdk_cap_t thread_a = 0;
    scdk_cap_t thread_b = 0;
    uint32_t state = SCDK_THREAD_NONE;
    uint64_t start_ticks;
    scdk_status_t status;

    scdk_log_write("test", "timer/preemption self-test start");

    status = scdk_timer_init(1000u);
    require_status("timer init", status, SCDK_OK);
    scdk_log_write("timer", "init ok");

    preempt_thread_a_started = false;
    preempt_thread_b_started = false;
    preempt_thread_a_done = false;
    preempt_thread_a_observed = false;
    preempt_thread_b_observed = false;
    preempt_thread_a_tick = 0;
    preempt_thread_b_tick = 0;

    status = scdk_task_create(SCDK_BOOT_CORE, 0, &task);
    require_status("timer preempt task create", status, SCDK_OK);

    status = scdk_thread_create(task, preempt_thread_a, 0, &thread_a);
    require_status("timer preempt thread A create", status, SCDK_OK);

    status = scdk_thread_create(task, preempt_thread_b, 0, &thread_b);
    require_status("timer preempt thread B create", status, SCDK_OK);

    status = scdk_thread_start(thread_a);
    require_status("timer preempt thread A start", status, SCDK_OK);

    status = scdk_thread_start(thread_b);
    require_status("timer preempt thread B start", status, SCDK_OK);

    start_ticks = scdk_timer_ticks();
    scdk_scheduler_run();
    scdk_timer_disable_preemption();

    if (!scdk_timer_tick_seen() || scdk_timer_ticks() == start_ticks) {
        scdk_panic("timer did not tick during preemption self-test");
    }
    scdk_log_write("timer", "tick ok");

    if (!preempt_thread_a_observed || !preempt_thread_b_observed) {
        scdk_panic("timer preemption did not switch both worker threads");
    }

    scdk_log_write("sched", "preempt thread A");
    scdk_log_write("sched", "preempt thread B");

    status = scdk_thread_state(thread_a, &state);
    require_status("timer preempt thread A dead state", status, SCDK_OK);
    if (state != SCDK_THREAD_DEAD) {
        scdk_panic("timer preempt thread A did not finish");
    }

    status = scdk_thread_state(thread_b, &state);
    require_status("timer preempt thread B dead state", status, SCDK_OK);
    if (state != SCDK_THREAD_DEAD) {
        scdk_panic("timer preempt thread B did not finish");
    }

    scdk_log_write("test", "timer/preemption: pass");
}

static scdk_status_t devmgr_test_endpoint_handler(scdk_cap_t endpoint,
                                                  struct scdk_message *msg,
                                                  void *context) {
    (void)endpoint;
    (void)msg;
    (void)context;
    return SCDK_OK;
}

static void run_devmgr_selftest(void) {
    scdk_cap_t endpoint = 0;
    scdk_cap_t looked_up = 0;
    scdk_cap_t target_endpoint = 0;
    scdk_cap_t device = 0;
    scdk_cap_t queue = 0;
    scdk_cap_t read_only_queue = 0;
    const struct scdk_cap_entry *entry = 0;
    struct scdk_message msg;
    scdk_status_t status;

    scdk_log_write("test", "devmgr self-test start");

    status = scdk_devmgr_service_init(&endpoint);
    require_status("devmgr service init", status, SCDK_OK);

    status = scdk_service_lookup(SCDK_SERVICE_DEVMGR, &looked_up);
    require_status("devmgr service lookup", status, SCDK_OK);
    if (looked_up != endpoint) {
        scdk_panic("devmgr service endpoint mismatch");
    }

    scdk_message_init(&msg,
                      0,
                      SCDK_SERVICE_DEVMGR,
                      SCDK_MSG_DEVICE_REGISTER);
    msg.arg0 = SCDK_DEVMGR_FAKE_DEVICE_ID;
    status = scdk_endpoint_call(looked_up, &msg);
    require_status("devmgr fake device register", status, SCDK_OK);

    device = (scdk_cap_t)msg.arg0;
    queue = (scdk_cap_t)msg.arg1;
    if (device == 0 || queue == 0) {
        scdk_panic("devmgr did not return device and queue caps");
    }

    status = scdk_cap_check(device,
                            SCDK_RIGHT_READ,
                            SCDK_OBJ_DEVICE,
                            0);
    require_status("devmgr device cap check", status, SCDK_OK);

    status = scdk_cap_check(queue,
                            SCDK_RIGHT_BIND,
                            SCDK_OBJ_DEVICE_QUEUE,
                            &entry);
    require_status("devmgr queue cap check", status, SCDK_OK);

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  devmgr_test_endpoint_handler,
                                  0,
                                  &target_endpoint);
    require_status("devmgr target endpoint create", status, SCDK_OK);

    scdk_message_init(&msg,
                      0,
                      SCDK_SERVICE_DEVMGR,
                      SCDK_MSG_DEVICE_QUEUE_BIND);
    msg.arg0 = queue;
    msg.arg1 = target_endpoint;
    status = scdk_endpoint_call(looked_up, &msg);
    require_status("devmgr queue bind", status, SCDK_OK);

    status = scdk_cap_create(entry->object_id,
                             SCDK_RIGHT_READ,
                             &read_only_queue);
    require_status("devmgr read-only queue cap create", status, SCDK_OK);

    scdk_message_init(&msg,
                      0,
                      SCDK_SERVICE_DEVMGR,
                      SCDK_MSG_DEVICE_QUEUE_BIND);
    msg.arg0 = read_only_queue;
    msg.arg1 = target_endpoint;
    status = scdk_endpoint_call(looked_up, &msg);
    require_status("devmgr unauthorized queue bind rejected", status, SCDK_ERR_PERM);
    scdk_log_write("devmgr", "unauthorized queue bind reject pass");

    status = scdk_revoke_capability(queue);
    require_status("devmgr queue cap revoke", status, SCDK_OK);

    scdk_message_init(&msg,
                      0,
                      SCDK_SERVICE_DEVMGR,
                      SCDK_MSG_DEVICE_QUEUE_BIND);
    msg.arg0 = queue;
    msg.arg1 = target_endpoint;
    status = scdk_endpoint_call(looked_up, &msg);
    require_status("devmgr revoked queue cap rejected", status, SCDK_ERR_NOENT);

    scdk_log_write("test", "devmgr: pass");
}

static void run_m30_architecture_review_selftest(void) {
    scdk_status_t status;

    scdk_log_write("m30", "architecture review start");

    status = scdk_cap_check(0, SCDK_RIGHT_READ, SCDK_OBJ_SERVICE, 0);
    require_status("m30 invalid cap rejected", status, SCDK_ERR_INVAL);
    scdk_log_write("m30", "capability boundary review pass");

    status = scdk_console_direct_access_audit();
    require_status("m30 hardware access audit", status, SCDK_OK);
    scdk_log_write("m30", "hardware access review pass");

    status = scdk_user_validate_range(0,
                                      sizeof(struct scdk_message),
                                      false);
    require_status("m30 null user pointer rejected", status, SCDK_ERR_BOUNDS);
    scdk_log_write("m30", "user pointer review pass");

    /*
     * The detailed ring/grant cases are exercised by the user ring, revoke,
     * and grant self-tests above. M30 keeps a final boot marker so the audit is
     * visible in serial logs.
     */
    scdk_log_write("m30", "ring/grant review pass");
    scdk_log_write("m30", "architecture review complete");
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
    run_initrd_selftest();
    run_tmpfs_selftest();
    run_vfs_selftest();
    run_loader_selftest();
    run_proc_selftest();
    run_m31_vfs_session_selftest();
    run_user_grant_selftest();
    run_console_grant_write_selftest();
    run_user_ring_selftest();
    run_revoke_selftest();
    run_fault_selftest();
    run_timer_preemption_selftest();
    run_devmgr_selftest();
    run_m30_architecture_review_selftest();

    scdk_log_write("test", "all core tests passed");
    return SCDK_OK;
}
