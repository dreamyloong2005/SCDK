// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/loader.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/endpoint.h>
#include <scdk/heap.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/service.h>
#include <scdk/task.h>

static scdk_status_t loader_close_file(scdk_cap_t vfs_endpoint,
                                       scdk_cap_t file) {
    struct scdk_message msg;

    if (vfs_endpoint == 0 || file == 0) {
        return SCDK_ERR_INVAL;
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_CLOSE);
    msg.arg0 = file;
    return scdk_endpoint_call(vfs_endpoint, &msg);
}

static scdk_status_t loader_read_file(scdk_cap_t vfs_endpoint,
                                      scdk_cap_t file,
                                      uint8_t *buffer,
                                      size_t size) {
    size_t offset = 0;

    if (vfs_endpoint == 0 || file == 0 || buffer == 0 || size == 0u) {
        return SCDK_ERR_INVAL;
    }

    while (offset < size) {
        struct scdk_message msg;
        scdk_status_t status;

        scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_READ);
        msg.arg0 = file;
        msg.arg1 = offset;
        msg.arg2 = (uint64_t)(uintptr_t)(buffer + offset);
        msg.arg3 = size - offset;

        status = scdk_endpoint_call(vfs_endpoint, &msg);
        if (status != SCDK_OK) {
            return status;
        }

        if (msg.arg0 == 0u || msg.arg0 > (uint64_t)(size - offset)) {
            return SCDK_ERR_BOUNDS;
        }

        offset += (size_t)msg.arg0;
    }

    return SCDK_OK;
}

static scdk_status_t loader_open_file(scdk_cap_t vfs_endpoint,
                                      const char *path,
                                      scdk_cap_t *out_file,
                                      size_t *out_size) {
    struct scdk_message msg;
    scdk_status_t status;

    if (vfs_endpoint == 0 || path == 0 || out_file == 0 || out_size == 0) {
        return SCDK_ERR_INVAL;
    }

    scdk_message_init(&msg, 0, SCDK_SERVICE_VFS, SCDK_MSG_OPEN);
    msg.arg0 = (uint64_t)(uintptr_t)path;
    msg.arg1 = 0;

    status = scdk_endpoint_call(vfs_endpoint, &msg);
    if (status != SCDK_OK) {
        return status;
    }

    if (msg.arg0 == 0u || msg.arg1 == 0u) {
        return SCDK_ERR_NOENT;
    }

    if (msg.arg1 > SCDK_LOADER_MAX_IMAGE_SIZE) {
        return SCDK_ERR_BOUNDS;
    }

    *out_file = (scdk_cap_t)msg.arg0;
    *out_size = (size_t)msg.arg1;
    return SCDK_OK;
}

static scdk_status_t loader_read_image(const char *path,
                                       uint8_t **out_image,
                                       size_t *out_size) {
    scdk_cap_t vfs_endpoint = 0;
    scdk_cap_t file = 0;
    uint8_t *image = 0;
    size_t image_size = 0;
    bool opened = false;
    scdk_status_t status;

    if (path == 0 || out_image == 0 || out_size == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_service_lookup(SCDK_SERVICE_VFS, &vfs_endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = loader_open_file(vfs_endpoint, path, &file, &image_size);
    if (status != SCDK_OK) {
        return status;
    }
    opened = true;

    image = scdk_kalloc(image_size);
    if (image == 0) {
        status = SCDK_ERR_NOMEM;
        goto out;
    }

    status = loader_read_file(vfs_endpoint, file, image, image_size);
    if (status != SCDK_OK) {
        scdk_kfree(image);
        image = 0;
        goto out;
    }

    *out_image = image;
    *out_size = image_size;

out:
    if (opened) {
        scdk_status_t close_status = loader_close_file(vfs_endpoint, file);
        if (status == SCDK_OK && close_status != SCDK_OK) {
            if (image != 0) {
                scdk_kfree(image);
                *out_image = 0;
                *out_size = 0;
            }
            status = close_status;
        }
    }

    return status;
}

scdk_status_t scdk_loader_load_from_vfs_with_endpoint(const char *path,
                                                      uint64_t hhdm_offset,
                                                      scdk_cap_t bootstrap_endpoint,
                                                      scdk_cap_t *out_task,
                                                      scdk_cap_t *out_main_thread) {
    uint8_t *image = 0;
    size_t image_size = 0;
    scdk_cap_t task = 0;
    scdk_cap_t address_space = 0;
    scdk_cap_t main_thread = 0;
    scdk_status_t status;

    if (path == 0 ||
        hhdm_offset == 0u ||
        bootstrap_endpoint == 0 ||
        out_task == 0 ||
        out_main_thread == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(bootstrap_endpoint,
                            SCDK_RIGHT_SEND,
                            SCDK_OBJ_ENDPOINT,
                            0);
    if (status != SCDK_OK) {
        return status;
    }

    scdk_log_write("loader", "loading %s", path);

    status = loader_read_image(path, &image, &image_size);
    if (status != SCDK_OK) {
        return status;
    }
    scdk_log_write("loader", "loaded %s", path);

    status = scdk_user_task_create(&task, &address_space, &main_thread);
    if (status != SCDK_OK) {
        scdk_kfree(image);
        return status;
    }

    if (address_space == 0 || main_thread == 0) {
        scdk_kfree(image);
        return SCDK_ERR_NOENT;
    }

    scdk_log_write("loader", "mapped %s", path);

    status = scdk_user_task_run_flat(task,
                                     image,
                                     image_size,
                                     bootstrap_endpoint,
                                     hhdm_offset);
    scdk_kfree(image);
    if (status != SCDK_OK) {
        return status;
    }

    *out_task = task;
    *out_main_thread = main_thread;
    scdk_log_write("loader", "%s start pass", path);
    return SCDK_OK;
}

scdk_status_t scdk_loader_load_from_vfs(const char *path,
                                        uint64_t hhdm_offset,
                                        scdk_cap_t *out_task,
                                        scdk_cap_t *out_main_thread) {
    scdk_cap_t console_endpoint = 0;
    scdk_status_t status;

    status = scdk_service_lookup(SCDK_SERVICE_CONSOLE, &console_endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    return scdk_loader_load_from_vfs_with_endpoint(path,
                                                  hhdm_offset,
                                                  console_endpoint,
                                                  out_task,
                                                  out_main_thread);
}
