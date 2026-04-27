// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/tmpfs.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/initrd.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/object.h>
#include <scdk/service.h>
#include <scdk/string.h>

struct scdk_tmpfs_file {
    scdk_object_id_t object_id;
    scdk_cap_t cap;
    struct scdk_initrd_file backing;
    bool open;
};

static struct scdk_tmpfs_file open_files[SCDK_TMPFS_MAX_OPEN_FILES];
static bool tmpfs_started;

static scdk_status_t tmpfs_slot_for_cap(scdk_cap_t file_cap,
                                        struct scdk_tmpfs_file **out_file) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_file == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(file_cap, SCDK_RIGHT_READ, SCDK_OBJ_FILE, &entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    struct scdk_tmpfs_file *file = object->payload;
    if (file == 0 || file->object_id != entry->object_id || !file->open) {
        return SCDK_ERR_NOENT;
    }

    *out_file = file;
    return SCDK_OK;
}

static struct scdk_tmpfs_file *alloc_open_file_slot(void) {
    for (uint32_t i = 0; i < SCDK_TMPFS_MAX_OPEN_FILES; i++) {
        if (!open_files[i].open) {
            return &open_files[i];
        }
    }

    return 0;
}

static scdk_status_t tmpfs_handle_open(struct scdk_message *msg) {
    const char *path;
    char bounded_path[SCDK_INITRD_MAX_PATH];
    uint64_t path_length;
    struct scdk_initrd_file backing;
    struct scdk_tmpfs_file *slot;
    scdk_object_id_t object_id = 0;
    scdk_cap_t cap = 0;
    scdk_status_t status;

    if (msg == 0 || msg->arg0 == 0u) {
        return SCDK_ERR_INVAL;
    }

    path = (const char *)(uintptr_t)msg->arg0;
    path_length = msg->arg1;
    if (path_length > SCDK_INITRD_MAX_PATH - 1u) {
        return SCDK_ERR_BOUNDS;
    }

    if (path_length == 0u) {
        status = scdk_initrd_find(path, &backing);
    } else {
        memcpy(bounded_path, path, (size_t)path_length);
        bounded_path[path_length] = '\0';
        status = scdk_initrd_find(bounded_path, &backing);
    }

    if (status != SCDK_OK) {
        return status;
    }

    slot = alloc_open_file_slot();
    if (slot == 0) {
        return SCDK_ERR_NOMEM;
    }

    status = scdk_object_create(SCDK_OBJ_FILE,
                                SCDK_BOOT_CORE,
                                0,
                                slot,
                                &object_id);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_cap_create(object_id, SCDK_RIGHT_READ, &cap);
    if (status != SCDK_OK) {
        slot->object_id = 0;
        return status;
    }

    slot->object_id = object_id;
    slot->cap = cap;
    slot->backing = backing;
    slot->open = true;

    msg->arg0 = cap;
    msg->arg1 = backing.size;
    return SCDK_OK;
}

static scdk_status_t tmpfs_handle_read(struct scdk_message *msg) {
    struct scdk_tmpfs_file *file = 0;
    uint64_t offset;
    uint64_t requested;
    uint64_t available;
    uint64_t to_copy;
    void *buffer;
    scdk_status_t status;

    if (msg == 0 || msg->arg2 == 0u) {
        return SCDK_ERR_INVAL;
    }

    status = tmpfs_slot_for_cap((scdk_cap_t)msg->arg0, &file);
    if (status != SCDK_OK) {
        return status;
    }

    offset = msg->arg1;
    requested = msg->arg3;
    buffer = (void *)(uintptr_t)msg->arg2;

    if (requested == 0u) {
        msg->arg0 = 0;
        return SCDK_OK;
    }

    if (offset > file->backing.size) {
        return SCDK_ERR_BOUNDS;
    }

    available = file->backing.size - offset;
    to_copy = requested < available ? requested : available;
    memcpy(buffer,
           (const uint8_t *)file->backing.data + offset,
           (size_t)to_copy);

    msg->arg0 = to_copy;
    return SCDK_OK;
}

static scdk_status_t tmpfs_handle_close(struct scdk_message *msg) {
    struct scdk_tmpfs_file *file = 0;
    scdk_status_t status;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    status = tmpfs_slot_for_cap((scdk_cap_t)msg->arg0, &file);
    if (status != SCDK_OK) {
        return status;
    }

    file->open = false;
    return SCDK_OK;
}

static scdk_status_t tmpfs_endpoint_handler(scdk_cap_t endpoint,
                                            struct scdk_message *msg,
                                            void *context) {
    (void)endpoint;
    (void)context;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    switch (msg->type) {
    case SCDK_MSG_OPEN:
        return tmpfs_handle_open(msg);
    case SCDK_MSG_READ:
        return tmpfs_handle_read(msg);
    case SCDK_MSG_CLOSE:
        return tmpfs_handle_close(msg);
    case SCDK_MSG_WRITE:
        return SCDK_ERR_NOTSUP;
    default:
        return SCDK_ERR_NOTSUP;
    }
}

scdk_status_t scdk_tmpfs_service_init(scdk_cap_t *out_endpoint) {
    scdk_cap_t endpoint = 0;
    scdk_status_t status;

    if (out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    if (tmpfs_started) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  tmpfs_endpoint_handler,
                                  0,
                                  &endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_service_register(SCDK_SERVICE_TMPFS, endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    tmpfs_started = true;
    *out_endpoint = endpoint;
    scdk_log_write("tmpfs", "service started");
    return SCDK_OK;
}
