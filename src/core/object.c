// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/object.h>

#define OBJECT_INDEX_MASK 0xffffffffull

static struct scdk_object object_table[SCDK_MAX_OBJECTS];
static uint32_t object_generations[SCDK_MAX_OBJECTS];

static scdk_object_id_t object_make_id(uint32_t index, uint32_t generation) {
    return ((uint64_t)generation << 32u) | (uint64_t)index;
}

static uint32_t object_id_index(scdk_object_id_t id) {
    return (uint32_t)(id & OBJECT_INDEX_MASK);
}

static uint32_t object_id_generation(scdk_object_id_t id) {
    return (uint32_t)(id >> 32u);
}

static uint32_t next_generation(uint32_t index) {
    uint32_t generation = object_generations[index] + 1u;

    if (generation == 0u) {
        generation = 1u;
    }

    object_generations[index] = generation;
    return generation;
}

scdk_status_t scdk_object_create(uint32_t type,
                                 uint32_t owner_core,
                                 uint32_t flags,
                                 void *payload,
                                 scdk_object_id_t *out_id) {
    if (out_id == 0 || type == SCDK_OBJ_NONE || owner_core >= SCDK_MAX_CORES) {
        return SCDK_ERR_INVAL;
    }

    for (uint32_t i = 0; i < SCDK_MAX_OBJECTS; i++) {
        if (object_table[i].type != SCDK_OBJ_NONE) {
            continue;
        }

        uint32_t generation = next_generation(i);
        scdk_object_id_t id = object_make_id(i, generation);

        object_table[i].id = id;
        object_table[i].type = type;
        object_table[i].generation = generation;
        object_table[i].owner_core = owner_core;
        object_table[i].flags = flags;
        object_table[i].payload = payload;

        *out_id = id;
        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_object_lookup(scdk_object_id_t id,
                                 const struct scdk_object **out_object) {
    if (out_object == 0 || id == 0) {
        return SCDK_ERR_INVAL;
    }

    uint32_t index = object_id_index(id);
    uint32_t generation = object_id_generation(id);

    if (index >= SCDK_MAX_OBJECTS || generation == 0u) {
        return SCDK_ERR_NOENT;
    }

    const struct scdk_object *object = &object_table[index];
    if (object->type == SCDK_OBJ_NONE ||
        object->generation != generation ||
        object->id != id) {
        return SCDK_ERR_NOENT;
    }

    *out_object = object;
    return SCDK_OK;
}
