// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/vfs.h>

#include <stdbool.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/service.h>
#include <scdk/string.h>

static scdk_cap_t root_tmpfs_endpoint;
static bool vfs_started;

static bool path_under_root(const char *path, uint64_t path_length) {
    if (path == 0) {
        return false;
    }

    if (path_length == 0u) {
        return path[0] == '/';
    }

    return path_length > 0u && path[0] == '/';
}

static scdk_status_t vfs_forward_to_tmpfs(struct scdk_message *msg) {
    scdk_status_t status;

    if (msg == 0 || root_tmpfs_endpoint == 0u) {
        return SCDK_ERR_INVAL;
    }

    msg->target = SCDK_SERVICE_TMPFS;
    status = scdk_endpoint_call(root_tmpfs_endpoint, msg);
    msg->target = SCDK_SERVICE_VFS;
    return status;
}

static scdk_status_t vfs_handle_open(struct scdk_message *msg) {
    if (msg == 0 || msg->arg0 == 0u) {
        return SCDK_ERR_INVAL;
    }

    if (!path_under_root((const char *)(uintptr_t)msg->arg0, msg->arg1)) {
        return SCDK_ERR_NOENT;
    }

    return vfs_forward_to_tmpfs(msg);
}

static scdk_status_t vfs_endpoint_handler(scdk_cap_t endpoint,
                                          struct scdk_message *msg,
                                          void *context) {
    (void)endpoint;
    (void)context;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    switch (msg->type) {
    case SCDK_MSG_OPEN:
        return vfs_handle_open(msg);
    case SCDK_MSG_READ:
    case SCDK_MSG_CLOSE:
        return vfs_forward_to_tmpfs(msg);
    case SCDK_MSG_WRITE:
        return SCDK_ERR_NOTSUP;
    default:
        return SCDK_ERR_NOTSUP;
    }
}

scdk_status_t scdk_vfs_service_init(scdk_cap_t *out_endpoint) {
    scdk_cap_t endpoint = 0;
    scdk_cap_t tmpfs_endpoint = 0;
    scdk_status_t status;

    if (out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    if (vfs_started) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_service_lookup(SCDK_SERVICE_TMPFS, &tmpfs_endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_cap_check(tmpfs_endpoint,
                            SCDK_RIGHT_SEND,
                            SCDK_OBJ_ENDPOINT,
                            0);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  vfs_endpoint_handler,
                                  0,
                                  &endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_service_register(SCDK_SERVICE_VFS, endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    root_tmpfs_endpoint = tmpfs_endpoint;
    vfs_started = true;
    *out_endpoint = endpoint;

    scdk_log_write("vfs", "service started");
    scdk_log_write("vfs", "mount tmpfs at / pass");
    return SCDK_OK;
}
