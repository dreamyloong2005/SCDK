// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/proc_service.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/loader.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/service.h>
#include <scdk/string.h>
#include <scdk/task.h>

static bool proc_started;
static uint64_t proc_hhdm_offset;

static scdk_status_t bounded_path_from_message(const struct scdk_message *msg,
                                               char *path,
                                               size_t path_size) {
    uint64_t length;

    if (msg == 0 || path == 0 || path_size == 0u || msg->arg0 == 0u) {
        return SCDK_ERR_INVAL;
    }

    length = msg->arg1;
    if (length == 0u || length >= path_size) {
        return SCDK_ERR_BOUNDS;
    }

    memcpy(path, (const char *)(uintptr_t)msg->arg0, (size_t)length);
    path[length] = '\0';
    return SCDK_OK;
}

static scdk_status_t proc_handle_spawn(struct scdk_message *msg) {
    char path[SCDK_PROC_MAX_PATH];
    scdk_cap_t task = 0;
    scdk_cap_t main_thread = 0;
    scdk_status_t status;

    status = bounded_path_from_message(msg, path, sizeof(path));
    if (status != SCDK_OK) {
        return status;
    }

    scdk_log_write("proc", "spawn %s", path);
    status = scdk_loader_load_from_vfs(path,
                                       proc_hhdm_offset,
                                       &task,
                                       &main_thread);
    if (status != SCDK_OK) {
        return status;
    }

    (void)main_thread;
    status = scdk_task_cleanup(task);
    if (status != SCDK_OK) {
        return status;
    }

    msg->arg0 = task;
    msg->arg1 = 0;
    scdk_log_write("proc", "process exited");
    return SCDK_OK;
}

static scdk_status_t proc_endpoint_handler(scdk_cap_t endpoint,
                                           struct scdk_message *msg,
                                           void *context) {
    (void)endpoint;
    (void)context;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    switch (msg->type) {
    case SCDK_MSG_PROCESS_SPAWN:
        return proc_handle_spawn(msg);
    case SCDK_MSG_PROCESS_EXIT:
        scdk_log_write("proc", "process exited");
        return SCDK_OK;
    case SCDK_MSG_PROCESS_WAIT_STUB:
        return SCDK_ERR_NOTSUP;
    default:
        return SCDK_ERR_NOTSUP;
    }
}

scdk_status_t scdk_proc_service_init(uint64_t hhdm_offset,
                                     scdk_cap_t *out_endpoint) {
    scdk_cap_t endpoint = 0;
    scdk_status_t status;

    if (out_endpoint == 0 || hhdm_offset == 0u) {
        return SCDK_ERR_INVAL;
    }

    if (proc_started) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  proc_endpoint_handler,
                                  0,
                                  &endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_service_register(SCDK_SERVICE_PROC, endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    proc_hhdm_offset = hhdm_offset;
    proc_started = true;
    *out_endpoint = endpoint;

    scdk_log_write("proc", "service started");
    return SCDK_OK;
}
