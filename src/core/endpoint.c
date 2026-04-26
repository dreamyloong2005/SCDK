// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/endpoint.h>
#include <scdk/object.h>
#include <scdk/service.h>

struct scdk_endpoint {
    scdk_object_id_t object_id;
    scdk_endpoint_handler_t handler;
    void *context;
};

struct scdk_service_slot {
    uint64_t service_id;
    scdk_cap_t endpoint;
};

static struct scdk_endpoint endpoint_table[SCDK_MAX_ENDPOINTS];
static struct scdk_service_slot service_table[SCDK_MAX_SERVICES];

scdk_status_t scdk_endpoint_create(uint32_t owner_core,
                                   scdk_endpoint_handler_t handler,
                                   void *context,
                                   scdk_cap_t *out_endpoint) {
    if (handler == 0 || out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    for (uint32_t i = 0; i < SCDK_MAX_ENDPOINTS; i++) {
        if (endpoint_table[i].object_id != 0) {
            continue;
        }

        scdk_object_id_t object_id = 0;
        scdk_status_t status = scdk_object_create(SCDK_OBJ_ENDPOINT,
                                                  owner_core,
                                                  0,
                                                  &endpoint_table[i],
                                                  &object_id);
        if (status != SCDK_OK) {
            return status;
        }

        endpoint_table[i].object_id = object_id;
        endpoint_table[i].handler = handler;
        endpoint_table[i].context = context;

        status = scdk_cap_create(object_id, SCDK_RIGHT_SEND | SCDK_RIGHT_BIND, out_endpoint);
        if (status != SCDK_OK) {
            endpoint_table[i].object_id = 0;
            endpoint_table[i].handler = 0;
            endpoint_table[i].context = 0;
            return status;
        }

        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_endpoint_call(scdk_cap_t endpoint,
                                 struct scdk_message *msg) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(endpoint, SCDK_RIGHT_SEND, SCDK_OBJ_ENDPOINT, &entry);
    if (status != SCDK_OK) {
        msg->status = (uint64_t)status;
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        msg->status = (uint64_t)status;
        return status;
    }

    struct scdk_endpoint *target = object->payload;
    if (target == 0 || target->object_id != entry->object_id || target->handler == 0) {
        msg->status = (uint64_t)SCDK_ERR_NOENT;
        return SCDK_ERR_NOENT;
    }

    msg->target = entry->object_id;
    status = target->handler(endpoint, msg, target->context);
    msg->status = (uint64_t)status;
    return status;
}

scdk_status_t scdk_service_register(uint64_t service_id,
                                    scdk_cap_t endpoint) {
    if (service_id == SCDK_SERVICE_NONE) {
        return SCDK_ERR_INVAL;
    }

    scdk_status_t status = scdk_cap_check(endpoint,
                                          SCDK_RIGHT_BIND,
                                          SCDK_OBJ_ENDPOINT,
                                          0);
    if (status != SCDK_OK) {
        return status;
    }

    for (uint32_t i = 0; i < SCDK_MAX_SERVICES; i++) {
        if (service_table[i].service_id == service_id) {
            return SCDK_ERR_BUSY;
        }
    }

    for (uint32_t i = 0; i < SCDK_MAX_SERVICES; i++) {
        if (service_table[i].service_id != SCDK_SERVICE_NONE) {
            continue;
        }

        service_table[i].service_id = service_id;
        service_table[i].endpoint = endpoint;
        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_service_lookup(uint64_t service_id,
                                  scdk_cap_t *out_endpoint) {
    if (service_id == SCDK_SERVICE_NONE || out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    for (uint32_t i = 0; i < SCDK_MAX_SERVICES; i++) {
        if (service_table[i].service_id != service_id) {
            continue;
        }

        *out_endpoint = service_table[i].endpoint;
        return SCDK_OK;
    }

    return SCDK_ERR_NOENT;
}
