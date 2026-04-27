// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/user_grant.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/object.h>
#include <scdk/string.h>
#include <scdk/user_ipc.h>

#define SCDK_USER_GRANT_RIGHTS_MASK (SCDK_RIGHT_READ | SCDK_RIGHT_WRITE | SCDK_RIGHT_MAP)

struct scdk_user_grant_slot {
    scdk_object_id_t object_id;
    scdk_cap_t source_task;
    scdk_cap_t target_endpoint;
    uintptr_t user_base;
    uint64_t length;
    uint64_t rights;
    uint32_t lifetime;
    bool revoked;
};

static struct scdk_user_grant_slot user_grants[SCDK_MAX_USER_GRANTS];

static scdk_status_t checked_user_range(uintptr_t user_base,
                                        uint64_t length,
                                        bool writable) {
    if (length == 0u || length > (uint64_t)((size_t)-1)) {
        return SCDK_ERR_INVAL;
    }

    return scdk_user_validate_range(user_base, (size_t)length, writable);
}

static scdk_status_t user_grant_lookup(scdk_cap_t grant_cap,
                                       uint64_t required_rights,
                                       struct scdk_user_grant_slot **out_slot) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_slot == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(grant_cap, required_rights, SCDK_OBJ_GRANT, &entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    struct scdk_user_grant_slot *slot = object->payload;
    if (slot == 0 || slot->object_id != entry->object_id) {
        return SCDK_ERR_NOENT;
    }

    if (slot->revoked) {
        return SCDK_ERR_PERM;
    }

    *out_slot = slot;
    return SCDK_OK;
}

static scdk_status_t check_grant_range(const struct scdk_user_grant_slot *slot,
                                       uint64_t offset,
                                       uint64_t length,
                                       uint64_t required_rights) {
    if (slot == 0 ||
        length == 0u ||
        required_rights == 0u ||
        (required_rights & ~SCDK_USER_GRANT_RIGHTS_MASK) != 0u) {
        return SCDK_ERR_INVAL;
    }

    if ((slot->rights & required_rights) != required_rights) {
        return SCDK_ERR_PERM;
    }

    if (offset > slot->length || length > slot->length - offset) {
        return SCDK_ERR_BOUNDS;
    }

    return SCDK_OK;
}

scdk_status_t scdk_user_grant_create(scdk_cap_t source_task,
                                     uintptr_t user_base,
                                     uint64_t length,
                                     uint64_t rights,
                                     scdk_cap_t target_endpoint,
                                     scdk_cap_t *out_grant) {
    bool writable;
    scdk_status_t status;

    if (source_task == 0 ||
        user_base == 0u ||
        length == 0u ||
        rights == 0u ||
        (rights & ~SCDK_USER_GRANT_RIGHTS_MASK) != 0u ||
        target_endpoint == 0 ||
        out_grant == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(source_task, SCDK_RIGHT_READ, SCDK_OBJ_TASK, 0);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_cap_check(target_endpoint, SCDK_RIGHT_SEND, SCDK_OBJ_ENDPOINT, 0);
    if (status != SCDK_OK) {
        return status;
    }

    writable = (rights & SCDK_RIGHT_WRITE) != 0u;
    status = checked_user_range(user_base, length, writable);
    if (status != SCDK_OK) {
        return status;
    }

    for (uint32_t i = 0; i < SCDK_MAX_USER_GRANTS; i++) {
        if (user_grants[i].object_id != 0) {
            continue;
        }

        scdk_object_id_t object_id = 0;
        status = scdk_object_create(SCDK_OBJ_GRANT,
                                    SCDK_BOOT_CORE,
                                    0,
                                    &user_grants[i],
                                    &object_id);
        if (status != SCDK_OK) {
            return status;
        }

        user_grants[i].object_id = object_id;
        user_grants[i].source_task = source_task;
        user_grants[i].target_endpoint = target_endpoint;
        user_grants[i].user_base = user_base;
        user_grants[i].length = length;
        user_grants[i].rights = rights;
        user_grants[i].lifetime = 1u;
        user_grants[i].revoked = false;

        status = scdk_cap_create(object_id, rights | SCDK_RIGHT_REVOKE, out_grant);
        if (status != SCDK_OK) {
            memset(&user_grants[i], 0, sizeof(user_grants[i]));
            return status;
        }

        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_user_grant_revoke(scdk_cap_t grant_cap) {
    struct scdk_user_grant_slot *slot = 0;
    scdk_status_t status;

    status = user_grant_lookup(grant_cap, SCDK_RIGHT_REVOKE, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    slot->revoked = true;
    return SCDK_OK;
}

scdk_status_t scdk_validate_grant_access(scdk_cap_t grant_cap,
                                         uint64_t offset,
                                         uint64_t length,
                                         uint64_t required_rights) {
    struct scdk_user_grant_slot *slot = 0;
    scdk_status_t status;

    status = user_grant_lookup(grant_cap, required_rights, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    return check_grant_range(slot, offset, length, required_rights);
}

scdk_status_t scdk_user_grant_copy_from(scdk_cap_t target_endpoint,
                                        scdk_cap_t grant_cap,
                                        uint64_t offset,
                                        void *kernel_dst,
                                        uint64_t length) {
    struct scdk_user_grant_slot *slot = 0;
    uintptr_t user_ptr;
    scdk_status_t status;

    if (target_endpoint == 0 || kernel_dst == 0) {
        return SCDK_ERR_INVAL;
    }

    status = user_grant_lookup(grant_cap, SCDK_RIGHT_READ, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    if (slot->target_endpoint != target_endpoint) {
        return SCDK_ERR_PERM;
    }

    status = check_grant_range(slot, offset, length, SCDK_RIGHT_READ);
    if (status != SCDK_OK) {
        return status;
    }

    user_ptr = slot->user_base + (uintptr_t)offset;
    return scdk_user_copy_from(user_ptr, kernel_dst, (size_t)length);
}
