// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/user_ipc.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/address_space.h>
#include <scdk/endpoint.h>
#include <scdk/message.h>
#include <scdk/mm.h>
#include <scdk/service.h>
#include <scdk/string.h>

static bool checked_user_end(uintptr_t user_ptr,
                             size_t size,
                             uintptr_t *out_last) {
    uintptr_t last;

    if (out_last == 0 || user_ptr == 0u || size == 0u) {
        return false;
    }

    if (user_ptr > ((uintptr_t)-1) - (size - 1u)) {
        return false;
    }

    last = user_ptr + size - 1u;
    if (user_ptr < SCDK_USER_VIRT_BASE || last >= SCDK_USER_VIRT_TOP) {
        return false;
    }

    *out_last = last;
    return true;
}

scdk_status_t scdk_user_validate_range(uintptr_t user_ptr,
                                       size_t size,
                                       bool writable) {
    uintptr_t last;
    uintptr_t page;

    if (!checked_user_end(user_ptr, size, &last)) {
        return SCDK_ERR_BOUNDS;
    }

    page = user_ptr & ~(uintptr_t)(SCDK_PAGE_SIZE - 1u);
    for (;;) {
        uint64_t phys = 0;
        uint64_t flags = 0;
        scdk_status_t status = scdk_vmm_virt_to_phys((uint64_t)page,
                                                     &phys,
                                                     &flags);
        (void)phys;

        if (status != SCDK_OK) {
            return status;
        }

        if ((flags & SCDK_VMM_MAP_USER) == 0u) {
            return SCDK_ERR_PERM;
        }

        if (writable && (flags & SCDK_VMM_MAP_WRITE) == 0u) {
            return SCDK_ERR_PERM;
        }

        if (page >= (last & ~(uintptr_t)(SCDK_PAGE_SIZE - 1u))) {
            break;
        }

        page += SCDK_PAGE_SIZE;
    }

    return SCDK_OK;
}

scdk_status_t scdk_user_copy_from(uintptr_t user_ptr,
                                  void *kernel_dst,
                                  size_t size) {
    scdk_status_t status;

    if (kernel_dst == 0 || size == 0u) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_user_validate_range(user_ptr, size, false);
    if (status != SCDK_OK) {
        return status;
    }

    memcpy(kernel_dst, (const void *)user_ptr, size);
    return SCDK_OK;
}

scdk_status_t scdk_user_copy_to(uintptr_t user_ptr,
                                const void *kernel_src,
                                size_t size) {
    scdk_status_t status;

    if (kernel_src == 0 || size == 0u) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_user_validate_range(user_ptr, size, true);
    if (status != SCDK_OK) {
        return status;
    }

    memcpy((void *)user_ptr, kernel_src, size);
    return SCDK_OK;
}

static scdk_status_t validate_user_message_target(scdk_cap_t endpoint_cap,
                                                  const struct scdk_message *msg) {
    scdk_cap_t service_endpoint = 0;
    scdk_status_t status;

    if (msg == 0 || msg->target == SCDK_SERVICE_NONE) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_service_lookup(msg->target, &service_endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    if (service_endpoint != endpoint_cap) {
        return SCDK_ERR_PERM;
    }

    return SCDK_OK;
}

static scdk_status_t prepare_user_buffer_arg(struct scdk_message *msg,
                                             char *buffer,
                                             size_t buffer_size) {
    uint64_t length;
    scdk_status_t status;

    if (msg == 0 || buffer == 0 || buffer_size == 0u) {
        return SCDK_ERR_INVAL;
    }

    if (msg->type != SCDK_MSG_WRITE &&
        msg->type != SCDK_MSG_PROCESS_SPAWN) {
        return SCDK_ERR_NOTSUP;
    }

    length = msg->arg1;
    if (msg->arg0 == 0u ||
        length == 0u ||
        length > (uint64_t)(buffer_size - 1u)) {
        return SCDK_ERR_BOUNDS;
    }

    status = scdk_user_copy_from((uintptr_t)msg->arg0,
                                 buffer,
                                 (size_t)length);
    if (status != SCDK_OK) {
        return status;
    }

    buffer[length] = '\0';
    msg->arg0 = (uint64_t)(uintptr_t)buffer;
    msg->arg1 = length;
    return SCDK_OK;
}

scdk_status_t scdk_sys_endpoint_call(scdk_cap_t endpoint_cap,
                                     uintptr_t user_message_ptr) {
    char payload_buffer[SCDK_USER_IPC_MAX_WRITE + 1u];
    struct scdk_message msg;
    scdk_status_t status;

    status = scdk_cap_check(endpoint_cap,
                            SCDK_RIGHT_SEND,
                            SCDK_OBJ_ENDPOINT,
                            0);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_user_copy_from(user_message_ptr, &msg, sizeof(msg));
    if (status != SCDK_OK) {
        return status;
    }

    status = validate_user_message_target(endpoint_cap, &msg);
    if (status != SCDK_OK) {
        msg.status = (uint64_t)status;
        (void)scdk_user_copy_to(user_message_ptr, &msg, sizeof(msg));
        return status;
    }

    switch (msg.type) {
    case SCDK_MSG_WRITE:
    case SCDK_MSG_PROCESS_SPAWN:
        status = prepare_user_buffer_arg(&msg, payload_buffer, sizeof(payload_buffer));
        if (status != SCDK_OK) {
            msg.status = (uint64_t)status;
            (void)scdk_user_copy_to(user_message_ptr, &msg, sizeof(msg));
            return status;
        }
        break;
    case SCDK_MSG_READ:
        break;
    default:
        msg.status = (uint64_t)SCDK_ERR_NOTSUP;
        (void)scdk_user_copy_to(user_message_ptr, &msg, sizeof(msg));
        return SCDK_ERR_NOTSUP;
    }

    status = scdk_endpoint_call(endpoint_cap, &msg);
    msg.status = (uint64_t)status;

    if (scdk_user_copy_to(user_message_ptr, &msg, sizeof(msg)) != SCDK_OK) {
        return SCDK_ERR_PERM;
    }

    return status;
}
