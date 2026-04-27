// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/address_space.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/heap.h>
#include <scdk/mm.h>
#include <scdk/object.h>

struct scdk_address_space {
    scdk_object_id_t object_id;
    uint64_t root_phys;
    uint32_t owner_core;
    uint32_t flags;
};

static bool is_page_aligned(uintptr_t value) {
    return (value & (SCDK_PAGE_SIZE - 1u)) == 0u;
}

static scdk_status_t validate_user_page(uintptr_t vaddr) {
    if (!is_page_aligned(vaddr)) {
        return SCDK_ERR_INVAL;
    }

    if (vaddr < SCDK_USER_VIRT_BASE ||
        vaddr > (SCDK_USER_VIRT_TOP - SCDK_PAGE_SIZE)) {
        return SCDK_ERR_BOUNDS;
    }

    return SCDK_OK;
}

static scdk_status_t lookup_aspace(scdk_cap_t aspace_cap,
                                   uint64_t required_rights,
                                   struct scdk_address_space **out_aspace) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_aspace == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(aspace_cap,
                            required_rights,
                            SCDK_OBJ_ADDRESS_SPACE,
                            &entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    struct scdk_address_space *aspace = object->payload;
    if (aspace == 0 ||
        aspace->object_id != entry->object_id ||
        aspace->root_phys == 0u) {
        return SCDK_ERR_NOENT;
    }

    *out_aspace = aspace;
    return SCDK_OK;
}

static scdk_status_t check_map_rights(scdk_cap_t aspace_cap,
                                      uint64_t flags,
                                      struct scdk_address_space **out_aspace) {
    uint64_t required = SCDK_RIGHT_MAP | SCDK_RIGHT_READ;

    if ((flags & SCDK_VMM_MAP_WRITE) != 0u) {
        required |= SCDK_RIGHT_WRITE;
    }

    return lookup_aspace(aspace_cap, required, out_aspace);
}

scdk_status_t scdk_address_space_create(scdk_cap_t *out_cap) {
    struct scdk_address_space *aspace;
    uint64_t root_phys = 0;
    scdk_object_id_t object_id = 0;
    scdk_status_t status;

    if (out_cap == 0) {
        return SCDK_ERR_INVAL;
    }

    aspace = scdk_alloc_object_storage(SCDK_OBJ_ADDRESS_SPACE, sizeof(*aspace));
    if (aspace == 0) {
        return SCDK_ERR_NOMEM;
    }

    status = scdk_vmm_create_root(&root_phys);
    if (status != SCDK_OK) {
        scdk_kfree(aspace);
        return status;
    }

    status = scdk_object_create(SCDK_OBJ_ADDRESS_SPACE,
                                SCDK_BOOT_CORE,
                                0,
                                aspace,
                                &object_id);
    if (status != SCDK_OK) {
        (void)scdk_page_free(root_phys);
        scdk_kfree(aspace);
        return status;
    }

    aspace->object_id = object_id;
    aspace->root_phys = root_phys;
    aspace->owner_core = SCDK_BOOT_CORE;
    aspace->flags = 0;

    status = scdk_cap_create(object_id,
                             SCDK_RIGHT_READ |
                             SCDK_RIGHT_WRITE |
                             SCDK_RIGHT_MAP |
                             SCDK_RIGHT_BIND,
                             out_cap);
    if (status != SCDK_OK) {
        return status;
    }

    return SCDK_OK;
}

scdk_status_t scdk_address_space_map(scdk_cap_t aspace_cap,
                                     uintptr_t vaddr,
                                     uintptr_t paddr,
                                     uint64_t flags) {
    struct scdk_address_space *aspace = 0;
    scdk_status_t status;

    status = check_map_rights(aspace_cap, flags, &aspace);
    if (status != SCDK_OK) {
        return status;
    }

    status = validate_user_page(vaddr);
    if (status != SCDK_OK) {
        return status;
    }

    if (!is_page_aligned(paddr)) {
        return SCDK_ERR_INVAL;
    }

    flags |= SCDK_VMM_MAP_USER;
    return scdk_vmm_map_page_in_root(aspace->root_phys,
                                     (uint64_t)vaddr,
                                     (uint64_t)paddr,
                                     flags);
}

scdk_status_t scdk_address_space_unmap(scdk_cap_t aspace_cap,
                                       uintptr_t vaddr) {
    struct scdk_address_space *aspace = 0;
    scdk_status_t status;

    status = lookup_aspace(aspace_cap,
                           SCDK_RIGHT_MAP | SCDK_RIGHT_READ,
                           &aspace);
    if (status != SCDK_OK) {
        return status;
    }

    status = validate_user_page(vaddr);
    if (status != SCDK_OK) {
        return status;
    }

    return scdk_vmm_unmap_page_in_root(aspace->root_phys, (uint64_t)vaddr);
}

scdk_status_t scdk_address_space_activate(scdk_cap_t aspace_cap) {
    struct scdk_address_space *aspace = 0;
    scdk_status_t status = lookup_aspace(aspace_cap,
                                         SCDK_RIGHT_READ,
                                         &aspace);

    if (status != SCDK_OK) {
        return status;
    }

    return scdk_vmm_activate_root(aspace->root_phys);
}

scdk_status_t scdk_address_space_translate(scdk_cap_t aspace_cap,
                                           uintptr_t vaddr,
                                           uint64_t *out_phys,
                                           uint64_t *out_flags) {
    struct scdk_address_space *aspace = 0;
    scdk_status_t status;

    if (out_phys == 0) {
        return SCDK_ERR_INVAL;
    }

    status = lookup_aspace(aspace_cap, SCDK_RIGHT_READ, &aspace);
    if (status != SCDK_OK) {
        return status;
    }

    status = validate_user_page(vaddr & ~(uintptr_t)(SCDK_PAGE_SIZE - 1u));
    if (status != SCDK_OK) {
        return status;
    }

    return scdk_vmm_virt_to_phys_in_root(aspace->root_phys,
                                         (uint64_t)vaddr,
                                         out_phys,
                                         out_flags);
}
