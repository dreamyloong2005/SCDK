// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/ring.h>

#include <scdk/object.h>

struct scdk_ring_slot {
    scdk_object_id_t object_id;
    struct scdk_ring ring;
    struct scdk_ring_desc sq_storage[SCDK_RING_MAX_ENTRIES];
    struct scdk_completion cq_storage[SCDK_RING_MAX_ENTRIES];
};

static struct scdk_ring_slot ring_table[SCDK_MAX_RINGS];

static uint32_t queue_count(uint32_t head, uint32_t tail) {
    return tail - head;
}

static scdk_status_t ring_lookup(scdk_cap_t cap,
                                 uint64_t required_rights,
                                 struct scdk_ring **out_ring) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_ring == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(cap, required_rights, SCDK_OBJ_RING, &entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    struct scdk_ring_slot *slot = object->payload;
    if (slot == 0 || slot->object_id != entry->object_id) {
        return SCDK_ERR_NOENT;
    }

    *out_ring = &slot->ring;
    return SCDK_OK;
}

scdk_status_t scdk_ring_create(uint32_t owner_core,
                               uint32_t entries,
                               scdk_cap_t bound_target,
                               scdk_cap_t *out_ring) {
    if (entries == 0u || entries > SCDK_RING_MAX_ENTRIES || out_ring == 0) {
        return SCDK_ERR_INVAL;
    }

    if (bound_target != 0) {
        scdk_status_t status = scdk_cap_check(bound_target,
                                              SCDK_RIGHT_SEND,
                                              SCDK_OBJ_ENDPOINT,
                                              0);
        if (status != SCDK_OK) {
            return status;
        }
    }

    for (uint32_t i = 0; i < SCDK_MAX_RINGS; i++) {
        if (ring_table[i].object_id != 0) {
            continue;
        }

        scdk_object_id_t object_id = 0;
        scdk_status_t status = scdk_object_create(SCDK_OBJ_RING,
                                                  owner_core,
                                                  0,
                                                  &ring_table[i],
                                                  &object_id);
        if (status != SCDK_OK) {
            return status;
        }

        ring_table[i].object_id = object_id;
        ring_table[i].ring.entries = entries;
        ring_table[i].ring.submit_head = 0;
        ring_table[i].ring.submit_tail = 0;
        ring_table[i].ring.complete_head = 0;
        ring_table[i].ring.complete_tail = 0;
        ring_table[i].ring.sq = ring_table[i].sq_storage;
        ring_table[i].ring.cq = ring_table[i].cq_storage;
        ring_table[i].ring.bound_target = bound_target;

        status = scdk_cap_create(object_id,
                                 SCDK_RIGHT_READ | SCDK_RIGHT_WRITE | SCDK_RIGHT_BIND,
                                 out_ring);
        if (status != SCDK_OK) {
            ring_table[i].object_id = 0;
            return status;
        }

        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_ring_bind_target(scdk_cap_t ring,
                                    scdk_cap_t target) {
    struct scdk_ring *target_ring = 0;
    scdk_status_t status;

    if (target == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(target, SCDK_RIGHT_SEND, SCDK_OBJ_ENDPOINT, 0);
    if (status != SCDK_OK) {
        return status;
    }

    status = ring_lookup(ring, SCDK_RIGHT_BIND, &target_ring);
    if (status != SCDK_OK) {
        return status;
    }

    target_ring->bound_target = target;
    return SCDK_OK;
}

scdk_status_t scdk_ring_bound_target(scdk_cap_t ring,
                                     scdk_cap_t *out_target) {
    struct scdk_ring *target_ring = 0;
    scdk_status_t status;

    if (out_target == 0) {
        return SCDK_ERR_INVAL;
    }

    status = ring_lookup(ring, SCDK_RIGHT_READ, &target_ring);
    if (status != SCDK_OK) {
        return status;
    }

    if (target_ring->bound_target != 0) {
        status = scdk_cap_check(target_ring->bound_target,
                                SCDK_RIGHT_SEND,
                                SCDK_OBJ_ENDPOINT,
                                0);
        if (status != SCDK_OK) {
            return status;
        }
    }

    *out_target = target_ring->bound_target;
    return SCDK_OK;
}

scdk_status_t scdk_ring_submit(scdk_cap_t ring,
                               const struct scdk_ring_desc *desc) {
    struct scdk_ring *target = 0;
    scdk_status_t status;

    if (desc == 0) {
        return SCDK_ERR_INVAL;
    }

    status = ring_lookup(ring, SCDK_RIGHT_WRITE, &target);
    if (status != SCDK_OK) {
        return status;
    }

    if (queue_count(target->submit_head, target->submit_tail) >= target->entries) {
        return SCDK_ERR_BUSY;
    }

    target->sq[target->submit_tail % target->entries] = *desc;
    target->submit_tail++;
    return SCDK_OK;
}

scdk_status_t scdk_ring_consume(scdk_cap_t ring,
                                struct scdk_ring_desc *out_desc) {
    struct scdk_ring *target = 0;
    scdk_status_t status;

    if (out_desc == 0) {
        return SCDK_ERR_INVAL;
    }

    status = ring_lookup(ring, SCDK_RIGHT_READ, &target);
    if (status != SCDK_OK) {
        return status;
    }

    if (target->submit_head == target->submit_tail) {
        return SCDK_ERR_NOENT;
    }

    *out_desc = target->sq[target->submit_head % target->entries];
    target->submit_head++;
    return SCDK_OK;
}

scdk_status_t scdk_ring_complete(scdk_cap_t ring,
                                 const struct scdk_completion *completion) {
    struct scdk_ring *target = 0;
    scdk_status_t status;

    if (completion == 0) {
        return SCDK_ERR_INVAL;
    }

    status = ring_lookup(ring, SCDK_RIGHT_WRITE, &target);
    if (status != SCDK_OK) {
        return status;
    }

    if (queue_count(target->complete_head, target->complete_tail) >= target->entries) {
        return SCDK_ERR_BUSY;
    }

    target->cq[target->complete_tail % target->entries] = *completion;
    target->complete_tail++;
    return SCDK_OK;
}

scdk_status_t scdk_ring_poll(scdk_cap_t ring,
                             struct scdk_completion *out_completion) {
    struct scdk_ring *target = 0;
    scdk_status_t status;

    if (out_completion == 0) {
        return SCDK_ERR_INVAL;
    }

    status = ring_lookup(ring, SCDK_RIGHT_READ, &target);
    if (status != SCDK_OK) {
        return status;
    }

    if (target->complete_head == target->complete_tail) {
        return SCDK_ERR_NOENT;
    }

    *out_completion = target->cq[target->complete_head % target->entries];
    target->complete_head++;
    return SCDK_OK;
}
