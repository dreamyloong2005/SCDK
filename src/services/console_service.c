// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/console.h>

#include <stddef.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/ring.h>
#include <scdk/service.h>
#include <scdk/string.h>
#include <scdk/user_grant.h>

#define SCDK_CONSOLE_RING_MAX_WRITE 128u

static scdk_status_t console_handle_write(struct scdk_message *msg) {
    const char *buffer = (const char *)(uintptr_t)msg->arg0;
    size_t length = (size_t)msg->arg1;

    if (buffer == 0) {
        return SCDK_ERR_INVAL;
    }

    if (length == 0u) {
        length = strlen(buffer);
    }

    return scdk_console_backend_write(buffer, length);
}

static scdk_status_t console_handle_grant_write(scdk_cap_t endpoint,
                                                struct scdk_message *msg) {
    char buffer[SCDK_CONSOLE_RING_MAX_WRITE + 1u];
    uint64_t length;
    scdk_status_t status;

    if (msg == 0 || msg->arg0 == 0u) {
        return SCDK_ERR_INVAL;
    }

    length = msg->arg2;
    if (length == 0u || length > SCDK_CONSOLE_RING_MAX_WRITE) {
        return SCDK_ERR_BOUNDS;
    }

    status = scdk_user_grant_copy_from(endpoint,
                                      (scdk_cap_t)msg->arg0,
                                      msg->arg1,
                                      buffer,
                                      length);
    if (status != SCDK_OK) {
        return status;
    }

    buffer[length] = '\0';
    return scdk_console_backend_write(buffer, (size_t)length);
}

static scdk_status_t console_handle_get_info(struct scdk_message *msg) {
    struct scdk_console_info info;
    scdk_status_t status;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_console_backend_get_info(&info);
    if (status != SCDK_OK) {
        return status;
    }

    msg->arg0 = info.columns;
    msg->arg1 = info.rows;
    msg->arg2 = ((uint64_t)info.cursor_x << 32u) | info.cursor_y;
    msg->arg3 = info.flags;
    return SCDK_OK;
}

static scdk_status_t console_handle_ring_process(scdk_cap_t endpoint,
                                                 struct scdk_message *msg) {
    char buffer[SCDK_CONSOLE_RING_MAX_WRITE + 1u];
    scdk_cap_t ring;
    scdk_cap_t bound = 0;
    uint32_t count;
    scdk_status_t status;

    if (msg == 0 || msg->arg0 == 0u || msg->arg1 == 0u) {
        return SCDK_ERR_INVAL;
    }

    ring = (scdk_cap_t)msg->arg0;
    count = (uint32_t)msg->arg1;
    if ((uint64_t)count != msg->arg1 || count > SCDK_RING_MAX_ENTRIES) {
        return SCDK_ERR_BOUNDS;
    }

    status = scdk_ring_bound_target(ring, &bound);
    if (status != SCDK_OK) {
        return status;
    }

    if (bound != endpoint) {
        return SCDK_ERR_PERM;
    }

    for (uint32_t i = 0; i < count; i++) {
        struct scdk_ring_desc desc;
        struct scdk_completion completion;
        struct scdk_message write_msg;
        scdk_status_t op_status;

        status = scdk_ring_consume(ring, &desc);
        if (status != SCDK_OK) {
            return status;
        }

        op_status = SCDK_OK;
        if (desc.op != SCDK_RING_OP_CONSOLE_WRITE ||
            desc.cap != endpoint ||
            desc.grant == 0u ||
            desc.length == 0u ||
            desc.length > SCDK_CONSOLE_RING_MAX_WRITE) {
            op_status = SCDK_ERR_INVAL;
        } else {
            op_status = scdk_user_grant_copy_from(endpoint,
                                                  (scdk_cap_t)desc.grant,
                                                  desc.offset,
                                                  buffer,
                                                  desc.length);
            if (op_status == SCDK_OK) {
                buffer[desc.length] = '\0';
                scdk_message_init(&write_msg, 0, SCDK_SERVICE_CONSOLE, SCDK_MSG_WRITE);
                write_msg.arg0 = (uint64_t)(uintptr_t)buffer;
                write_msg.arg1 = desc.length;
                op_status = console_handle_write(&write_msg);
            }
        }

        completion.op = desc.op;
        completion.status = (uint64_t)op_status;
        completion.result0 = op_status == SCDK_OK ? desc.length : 0u;
        completion.result1 = desc.flags;

        status = scdk_ring_complete(ring, &completion);
        if (status != SCDK_OK) {
            return status;
        }

        if (op_status != SCDK_OK) {
            return op_status;
        }
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
    case SCDK_MSG_CONSOLE_WRITE:
        return console_handle_grant_write(endpoint, msg);
    case SCDK_MSG_CONSOLE_CLEAR:
        return scdk_console_backend_clear();
    case SCDK_MSG_CONSOLE_GET_INFO:
        return console_handle_get_info(msg);
    case SCDK_MSG_CONSOLE_BIND_OUTPUT_RING:
        return SCDK_ERR_NOTSUP;
    case SCDK_MSG_RING_PROCESS:
        return console_handle_ring_process(endpoint, msg);
    default:
        return SCDK_ERR_NOTSUP;
    }
}

scdk_status_t scdk_console_service_init(scdk_cap_t *out_endpoint) {
    scdk_cap_t endpoint = 0;
    scdk_status_t status;
    struct scdk_console_info info;

    if (out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_console_backend_init();
    if (status != SCDK_OK) {
        return status;
    }
    scdk_log_write("console", "backend init ok");
    scdk_log_write("console", "framebuffer text backend ok");
    if (scdk_console_backend_get_info(&info) == SCDK_OK &&
        (info.flags & SCDK_CONSOLE_INFO_SERIAL_MIRROR) != 0u) {
        scdk_log_write("console", "serial mirror backend ok");
    } else {
        scdk_log_write("console", "serial mirror backend unavailable");
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
    scdk_log_write("console", "frontend ready");
    scdk_log_write("console", "service endpoint registered");
    return SCDK_OK;
}
