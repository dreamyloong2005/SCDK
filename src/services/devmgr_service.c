// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/devmgr.h>

#include <stdbool.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/object.h>
#include <scdk/service.h>

struct scdk_devmgr_device {
    scdk_object_id_t object_id;
    scdk_cap_t cap;
    uint64_t device_id;
    bool present;
};

struct scdk_devmgr_queue {
    scdk_object_id_t object_id;
    scdk_cap_t cap;
    scdk_object_id_t device_id;
    scdk_cap_t bound_target;
    bool present;
};

static struct scdk_devmgr_device devices[SCDK_DEVMGR_MAX_DEVICES];
static struct scdk_devmgr_queue queues[SCDK_DEVMGR_MAX_QUEUES];
static bool devmgr_started;

static struct scdk_devmgr_device *alloc_device_slot(void) {
    for (uint32_t i = 0; i < SCDK_DEVMGR_MAX_DEVICES; i++) {
        if (!devices[i].present) {
            return &devices[i];
        }
    }

    return 0;
}

static struct scdk_devmgr_queue *alloc_queue_slot(void) {
    for (uint32_t i = 0; i < SCDK_DEVMGR_MAX_QUEUES; i++) {
        if (!queues[i].present) {
            return &queues[i];
        }
    }

    return 0;
}

static scdk_status_t devmgr_queue_from_cap(scdk_cap_t queue_cap,
                                           uint64_t required_rights,
                                           struct scdk_devmgr_queue **out_queue) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_queue == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(queue_cap,
                            required_rights,
                            SCDK_OBJ_DEVICE_QUEUE,
                            &entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    struct scdk_devmgr_queue *queue = object->payload;
    if (queue == 0 || queue->object_id != entry->object_id || !queue->present) {
        return SCDK_ERR_NOENT;
    }

    *out_queue = queue;
    return SCDK_OK;
}

static scdk_status_t devmgr_register_fake_device(struct scdk_message *msg) {
    struct scdk_devmgr_device *device;
    struct scdk_devmgr_queue *queue;
    scdk_object_id_t device_object = 0;
    scdk_object_id_t queue_object = 0;
    scdk_cap_t device_cap = 0;
    scdk_cap_t queue_cap = 0;
    scdk_status_t status;

    if (msg == 0 || msg->arg0 != SCDK_DEVMGR_FAKE_DEVICE_ID) {
        return SCDK_ERR_INVAL;
    }

    device = alloc_device_slot();
    queue = alloc_queue_slot();
    if (device == 0 || queue == 0) {
        return SCDK_ERR_NOMEM;
    }

    status = scdk_object_create(SCDK_OBJ_DEVICE,
                                SCDK_BOOT_CORE,
                                0,
                                device,
                                &device_object);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_cap_create(device_object,
                             SCDK_RIGHT_READ | SCDK_RIGHT_BIND | SCDK_RIGHT_REVOKE,
                             &device_cap);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_create(SCDK_OBJ_DEVICE_QUEUE,
                                SCDK_BOOT_CORE,
                                0,
                                queue,
                                &queue_object);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_cap_create(queue_object,
                             SCDK_RIGHT_READ |
                                 SCDK_RIGHT_WRITE |
                                 SCDK_RIGHT_BIND |
                                 SCDK_RIGHT_REVOKE,
                             &queue_cap);
    if (status != SCDK_OK) {
        return status;
    }

    device->object_id = device_object;
    device->cap = device_cap;
    device->device_id = SCDK_DEVMGR_FAKE_DEVICE_ID;
    device->present = true;

    queue->object_id = queue_object;
    queue->cap = queue_cap;
    queue->device_id = device_object;
    queue->bound_target = 0;
    queue->present = true;

    msg->arg0 = device_cap;
    msg->arg1 = queue_cap;

    scdk_log_write("devmgr", "fake device registered");
    scdk_log_write("devmgr", "queue capability created");
    return SCDK_OK;
}

static scdk_status_t devmgr_bind_queue(struct scdk_message *msg) {
    struct scdk_devmgr_queue *queue = 0;
    scdk_cap_t queue_cap;
    scdk_cap_t target_endpoint;
    scdk_status_t status;

    if (msg == 0 || msg->arg0 == 0u || msg->arg1 == 0u) {
        return SCDK_ERR_INVAL;
    }

    queue_cap = (scdk_cap_t)msg->arg0;
    target_endpoint = (scdk_cap_t)msg->arg1;

    status = scdk_cap_check(target_endpoint,
                            SCDK_RIGHT_SEND,
                            SCDK_OBJ_ENDPOINT,
                            0);
    if (status != SCDK_OK) {
        return status;
    }

    status = devmgr_queue_from_cap(queue_cap, SCDK_RIGHT_BIND, &queue);
    if (status != SCDK_OK) {
        return status;
    }

    queue->bound_target = target_endpoint;
    scdk_log_write("devmgr", "queue bind pass");
    return SCDK_OK;
}

static scdk_status_t devmgr_endpoint_handler(scdk_cap_t endpoint,
                                             struct scdk_message *msg,
                                             void *context) {
    (void)endpoint;
    (void)context;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    switch (msg->type) {
    case SCDK_MSG_DEVICE_REGISTER:
        return devmgr_register_fake_device(msg);
    case SCDK_MSG_DEVICE_QUEUE_BIND:
        return devmgr_bind_queue(msg);
    default:
        return SCDK_ERR_NOTSUP;
    }
}

scdk_status_t scdk_devmgr_service_init(scdk_cap_t *out_endpoint) {
    scdk_cap_t endpoint = 0;
    scdk_status_t status;

    if (out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    if (devmgr_started) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  devmgr_endpoint_handler,
                                  0,
                                  &endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_service_register(SCDK_SERVICE_DEVMGR, endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    devmgr_started = true;
    *out_endpoint = endpoint;

    scdk_log_write("devmgr", "service started");
    return SCDK_OK;
}
