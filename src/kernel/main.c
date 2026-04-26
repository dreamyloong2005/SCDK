// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <limine.h>

#include <scdk/capability.h>
#include <scdk/console_service.h>
#include <scdk/endpoint.h>
#include <scdk/framebuffer.h>
#include <scdk/grant.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/object.h>
#include <scdk/panic.h>
#include <scdk/ring.h>
#include <scdk/serial.h>
#include <scdk/service.h>

#ifndef SCDK_VERSION
#define SCDK_VERSION "0.0.0-unknown"
#endif

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[2] = LIMINE_REQUESTS_END_MARKER;

static void log_bootloader_info(void) {
    struct limine_bootloader_info_response *response = bootloader_info_request.response;

    if (response == 0) {
        scdk_log_warn("bootloader info unavailable");
        return;
    }

    scdk_log_info("bootloader: %s %s", response->name, response->version);
}

static void log_memory_map(void) {
    struct limine_memmap_response *response = memmap_request.response;

    if (response == 0) {
        scdk_panic("boot memory map missing");
    }

    scdk_log_write("boot", "memory map received");
    scdk_log_info("memory map entries: %llu", response->entry_count);
}

static void log_address_info(void) {
    if (hhdm_request.response != 0) {
        scdk_log_info("hhdm offset: 0x%llx", hhdm_request.response->offset);
    }

    if (executable_address_request.response != 0) {
        scdk_log_info("kernel physical base: 0x%llx",
                      executable_address_request.response->physical_base);
        scdk_log_info("kernel virtual base: 0x%llx",
                      executable_address_request.response->virtual_base);
    }
}

static void init_framebuffer(void) {
    struct limine_framebuffer_response *response = framebuffer_request.response;

    if (response == 0 || response->framebuffer_count == 0) {
        scdk_panic("boot framebuffer missing");
    }

    if (!scdk_framebuffer_init(response->framebuffers[0])) {
        scdk_panic("boot framebuffer init failed");
    }

    scdk_framebuffer_draw_test_pattern();
    scdk_log_write("boot", "framebuffer ok");
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

static void run_capability_core_selftest(void) {
    scdk_object_id_t object_id = 0;
    scdk_cap_t cap = 0;
    const struct scdk_object *object = 0;
    const struct scdk_cap_entry *entry = 0;
    scdk_status_t status;

    scdk_log_write("test", "capability core self-test start");

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

    scdk_log_write("boot", "grant core initialized");
}

__attribute__((noreturn)) static void idle_forever(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kmain(void) {
    bool serial_ok = scdk_serial_init();

    scdk_log_write("boot", "SCDK kernel entered");
    if (!serial_ok) {
        scdk_panic("boot serial init failed");
    }

    scdk_log_write("boot", "SCDK %s", SCDK_VERSION);
    scdk_log_write("boot", "serial ok");
    scdk_log_info("log subsystem ok");

    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        scdk_panic("boot unsupported Limine base revision");
    }

    log_bootloader_info();
    init_framebuffer();
    log_memory_map();
    log_address_info();
    run_capability_core_selftest();
    run_endpoint_message_selftest();
    run_ring_selftest();
    run_grant_selftest();

    scdk_log_write("boot", "milestone 7 complete");
    idle_forever();
}
