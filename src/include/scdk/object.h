// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_OBJECT_H
#define SCDK_OBJECT_H

#include <stdint.h>

#include <scdk/types.h>

#define SCDK_MAX_OBJECTS 4096u

typedef uint64_t scdk_object_id_t;

enum scdk_object_type {
    SCDK_OBJ_NONE = 0,
    SCDK_OBJ_FRAME,
    SCDK_OBJ_ADDRESS_SPACE,
    SCDK_OBJ_TASK,
    SCDK_OBJ_THREAD,
    SCDK_OBJ_ENDPOINT,
    SCDK_OBJ_RING,
    SCDK_OBJ_GRANT,
    SCDK_OBJ_SERVICE,
    SCDK_OBJ_DEVICE,
    SCDK_OBJ_DEVICE_QUEUE,
};

struct scdk_object {
    scdk_object_id_t id;
    uint32_t type;
    uint32_t generation;
    uint32_t owner_core;
    uint32_t flags;
    void *payload;
};

/*
 * Control-plane: create a kernel object metadata entry.
 * Requires no capability during bootstrap; future callers should hold creation
 * authority for the target object type.
 */
scdk_status_t scdk_object_create(uint32_t type,
                                 uint32_t owner_core,
                                 uint32_t flags,
                                 void *payload,
                                 scdk_object_id_t *out_id);

/*
 * Control-plane: look up object metadata by ID.
 * Requires no capability during bootstrap. Callers must not dereference payload
 * across subsystem boundaries without a separate capability check.
 */
scdk_status_t scdk_object_lookup(scdk_object_id_t id,
                                 const struct scdk_object **out_object);

#endif
