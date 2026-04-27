// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <limine.h>

#include <stdbool.h>
#include <stdint.h>

#include <scdk/framebuffer.h>
#include <scdk/initrd.h>
#include <scdk/log.h>
#include <scdk/panic.h>
#include <scdk/selftest.h>
#include <scdk/serial.h>

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
    .response = 0,
    .internal_module_count = 0,
    .internal_modules = 0
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

__attribute__((noreturn)) static void idle_forever(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kmain(void) {
    bool serial_ok = scdk_serial_init();
    uint64_t hhdm_offset = 0;
    scdk_status_t status;

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
    if (hhdm_request.response != 0) {
        hhdm_offset = hhdm_request.response->offset;
    }

    scdk_initrd_set_limine_response(module_request.response);
    scdk_selftest_set_boot_context(memmap_request.response, hhdm_offset);
    status = scdk_run_core_selftests();
    if (status != SCDK_OK) {
        scdk_panic("core self-tests failed: %lld", (long long)status);
    }

    scdk_log_write("boot", "milestone 22 complete");
    idle_forever();
}
