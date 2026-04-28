// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/capability.h>

#define CAP_INDEX_MASK 0xffffffffull

static struct scdk_cap_entry cap_table[SCDK_MAX_CAPS];
static uint32_t cap_generations[SCDK_MAX_CAPS];

static scdk_cap_t cap_make_token(uint32_t index, uint32_t generation) {
    return ((uint64_t)generation << 32u) | (uint64_t)index;
}

static uint32_t cap_index(scdk_cap_t cap) {
    return (uint32_t)(cap & CAP_INDEX_MASK);
}

static uint32_t cap_generation(scdk_cap_t cap) {
    return (uint32_t)(cap >> 32u);
}

static uint32_t next_cap_generation(uint32_t index) {
    uint32_t generation = cap_generations[index] + 1u;

    if (generation == 0u) {
        generation = 1u;
    }

    cap_generations[index] = generation;
    return generation;
}

static scdk_status_t validate_cap_slot(scdk_cap_t cap,
                                       const struct scdk_cap_entry **out_entry) {
    if (out_entry == 0 || cap == 0) {
        return SCDK_ERR_INVAL;
    }

    uint32_t index = cap_index(cap);
    uint32_t generation = cap_generation(cap);

    if (index >= SCDK_MAX_CAPS ||
        generation == 0u ||
        cap_generations[index] != generation ||
        cap_table[index].object_id == 0 ||
        cap_table[index].revoked != 0u) {
        return SCDK_ERR_NOENT;
    }

    *out_entry = &cap_table[index];
    return SCDK_OK;
}

scdk_status_t scdk_cap_create(scdk_object_id_t object_id,
                              uint64_t rights,
                              scdk_cap_t *out_cap) {
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_cap == 0 || rights == 0u) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_object_lookup(object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    for (uint32_t i = 0; i < SCDK_MAX_CAPS; i++) {
        if (cap_table[i].object_id != 0 && cap_table[i].revoked == 0u) {
            continue;
        }

        uint32_t cap_gen = next_cap_generation(i);
        cap_table[i].object_id = object->id;
        cap_table[i].generation = object->generation;
        cap_table[i].object_type = object->type;
        cap_table[i].revoked = 0u;
        cap_table[i].rights = rights;

        *out_cap = cap_make_token(i, cap_gen);
        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_cap_lookup(scdk_cap_t cap,
                              const struct scdk_cap_entry **out_entry) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status = validate_cap_slot(cap, &entry);

    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    if (object->generation != entry->generation ||
        object->type != entry->object_type) {
        return SCDK_ERR_NOENT;
    }

    *out_entry = entry;
    return SCDK_OK;
}

scdk_status_t scdk_cap_check(scdk_cap_t cap,
                             uint64_t required_rights,
                             uint32_t expected_type,
                             const struct scdk_cap_entry **out_entry) {
    const struct scdk_cap_entry *entry = 0;
    scdk_status_t status;

    if (required_rights == 0u) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_lookup(cap, &entry);
    if (status != SCDK_OK) {
        return status;
    }

    if (expected_type != SCDK_OBJ_NONE && entry->object_type != expected_type) {
        return SCDK_ERR_PERM;
    }

    if ((entry->rights & required_rights) != required_rights) {
        return SCDK_ERR_PERM;
    }

    if (out_entry != 0) {
        *out_entry = entry;
    }

    return SCDK_OK;
}

scdk_status_t scdk_cap_revoke_stub(scdk_cap_t cap) {
    const struct scdk_cap_entry *entry = 0;
    scdk_status_t status = scdk_cap_lookup(cap, &entry);

    if (status != SCDK_OK) {
        return status;
    }

    return scdk_cap_revoke(cap);
}

scdk_status_t scdk_cap_revoke(scdk_cap_t cap) {
    const struct scdk_cap_entry *entry = 0;
    scdk_status_t status = scdk_cap_lookup(cap, &entry);

    if (status != SCDK_OK) {
        return status;
    }

    uint32_t index = cap_index(cap);
    cap_table[index].revoked = 1u;
    (void)next_cap_generation(index);
    return SCDK_OK;
}
