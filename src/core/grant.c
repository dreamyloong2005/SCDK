// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/grant.h>

#include <scdk/object.h>

#define SCDK_GRANT_RIGHTS_MASK (SCDK_RIGHT_READ | SCDK_RIGHT_WRITE | SCDK_RIGHT_MAP)

struct scdk_grant_slot {
    scdk_object_id_t object_id;
    struct scdk_grant grant;
};

static struct scdk_grant_slot grant_table[SCDK_MAX_GRANTS];

static scdk_status_t grant_lookup(scdk_cap_t cap,
                                  uint64_t required_rights,
                                  struct scdk_grant **out_grant) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_grant == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(cap, required_rights, SCDK_OBJ_GRANT, &entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    struct scdk_grant_slot *slot = object->payload;
    if (slot == 0 || slot->object_id != entry->object_id) {
        return SCDK_ERR_NOENT;
    }

    *out_grant = &slot->grant;
    return SCDK_OK;
}

static scdk_status_t grant_check_range(const struct scdk_grant *grant,
                                       uint64_t required_rights,
                                       uint64_t offset,
                                       uint64_t length) {
    if (grant == 0 ||
        required_rights == 0u ||
        (required_rights & ~SCDK_GRANT_RIGHTS_MASK) != 0u ||
        length == 0u) {
        return SCDK_ERR_INVAL;
    }

    if ((grant->rights & required_rights) != required_rights) {
        return SCDK_ERR_PERM;
    }

    if (offset < grant->offset) {
        return SCDK_ERR_BOUNDS;
    }

    uint64_t relative_offset = offset - grant->offset;
    if (relative_offset > grant->length || length > grant->length - relative_offset) {
        return SCDK_ERR_BOUNDS;
    }

    return SCDK_OK;
}

scdk_status_t scdk_grant_create(void *base,
                                uint64_t length,
                                uint64_t rights,
                                uint32_t lifetime,
                                scdk_cap_t *out_grant) {
    if (base == 0 ||
        length == 0u ||
        rights == 0u ||
        (rights & ~SCDK_GRANT_RIGHTS_MASK) != 0u ||
        out_grant == 0) {
        return SCDK_ERR_INVAL;
    }

    for (uint32_t i = 0; i < SCDK_MAX_GRANTS; i++) {
        if (grant_table[i].object_id != 0) {
            continue;
        }

        scdk_object_id_t object_id = 0;
        scdk_status_t status = scdk_object_create(SCDK_OBJ_GRANT,
                                                  SCDK_BOOT_CORE,
                                                  0,
                                                  &grant_table[i],
                                                  &object_id);
        if (status != SCDK_OK) {
            return status;
        }

        grant_table[i].object_id = object_id;
        grant_table[i].grant.frame_cap = 0;
        grant_table[i].grant.base = (uintptr_t)base;
        grant_table[i].grant.offset = 0;
        grant_table[i].grant.length = length;
        grant_table[i].grant.rights = rights;
        grant_table[i].grant.lifetime = lifetime;
        grant_table[i].grant.refcount = 1;

        status = scdk_cap_create(object_id, rights, out_grant);
        if (status != SCDK_OK) {
            grant_table[i].object_id = 0;
            return status;
        }

        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_grant_check(scdk_cap_t grant,
                               uint64_t required_rights,
                               uint64_t offset,
                               uint64_t length) {
    struct scdk_grant *target = 0;
    scdk_status_t status = grant_lookup(grant, required_rights, &target);

    if (status != SCDK_OK) {
        return status;
    }

    return grant_check_range(target, required_rights, offset, length);
}

scdk_status_t scdk_grant_resolve(scdk_cap_t grant,
                                 uint64_t required_rights,
                                 uint64_t offset,
                                 uint64_t length,
                                 void **out_ptr) {
    struct scdk_grant *target = 0;
    scdk_status_t status;

    if (out_ptr == 0) {
        return SCDK_ERR_INVAL;
    }

    status = grant_lookup(grant, required_rights, &target);
    if (status != SCDK_OK) {
        return status;
    }

    status = grant_check_range(target, required_rights, offset, length);
    if (status != SCDK_OK) {
        return status;
    }

    *out_ptr = (void *)(target->base + (offset - target->offset));
    return SCDK_OK;
}
