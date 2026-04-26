// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/console_service.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/framebuffer.h>
#include <scdk/message.h>
#include <scdk/serial.h>
#include <scdk/service.h>
#include <scdk/string.h>

static scdk_status_t console_handle_write(struct scdk_message *msg) {
    const char *buffer = (const char *)(uintptr_t)msg->arg0;
    size_t length = (size_t)msg->arg1;
    bool needs_newline;

    if (buffer == 0) {
        return SCDK_ERR_INVAL;
    }

    if (length == 0u) {
        length = strlen(buffer);
    }

    needs_newline = length == 0u || buffer[length - 1u] != '\n';

    for (size_t i = 0; i < length; i++) {
        scdk_serial_write_char(buffer[i]);
    }

    if (needs_newline) {
        scdk_serial_write_char('\n');
    }

    scdk_framebuffer_console_write(buffer, length);
    if (needs_newline) {
        scdk_framebuffer_console_write("\n", 1u);
    }

    return SCDK_OK;
}

static scdk_status_t console_endpoint_handler(scdk_cap_t endpoint,
                                              struct scdk_message *msg,
                                              void *context) {
    (void)endpoint;
    (void)context;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    switch (msg->type) {
    case SCDK_MSG_WRITE:
        return console_handle_write(msg);
    default:
        return SCDK_ERR_NOTSUP;
    }
}

scdk_status_t scdk_console_service_init(scdk_cap_t *out_endpoint) {
    scdk_cap_t endpoint = 0;
    scdk_status_t status;

    if (out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  console_endpoint_handler,
                                  0,
                                  &endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_service_register(SCDK_SERVICE_CONSOLE, endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    *out_endpoint = endpoint;
    return SCDK_OK;
}
