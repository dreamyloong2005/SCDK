// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/user_ring.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/object.h>
#include <scdk/ring.h>
#include <scdk/string.h>
#include <scdk/task.h>
#include <scdk/user_ipc.h>

struct scdk_user_ring_slot {
    bool in_use;
    scdk_cap_t source_task;
    scdk_cap_t ring_cap;
    scdk_cap_t target_endpoint;
};

static struct scdk_user_ring_slot user_rings[SCDK_MAX_USER_RINGS];

static scdk_status_t validate_count(uint32_t count) {
    if (count == 0u || count > SCDK_RING_MAX_ENTRIES) {
        return SCDK_ERR_BOUNDS;
    }

    return SCDK_OK;
}

static scdk_status_t user_ring_slot_for(scdk_cap_t source_task,
                                        scdk_cap_t ring_cap,
                                        struct scdk_user_ring_slot **out_slot) {
    scdk_status_t status;

    if (source_task == 0 || ring_cap == 0 || out_slot == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(source_task, SCDK_RIGHT_READ, SCDK_OBJ_TASK, 0);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_cap_check(ring_cap, SCDK_RIGHT_READ, SCDK_OBJ_RING, 0);
    if (status != SCDK_OK) {
        return status;
    }

    for (uint32_t i = 0; i < SCDK_MAX_USER_RINGS; i++) {
        if (!user_rings[i].in_use ||
            user_rings[i].source_task != source_task ||
            user_rings[i].ring_cap != ring_cap) {
            continue;
        }

        *out_slot = &user_rings[i];
        return SCDK_OK;
    }

    return SCDK_ERR_NOENT;
}

scdk_status_t scdk_user_ring_create(scdk_cap_t source_task,
                                    uint32_t entries,
                                    scdk_cap_t *out_ring) {
    scdk_status_t status;

    if (source_task == 0 || out_ring == 0) {
        return SCDK_ERR_INVAL;
    }

    status = validate_count(entries);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_cap_check(source_task, SCDK_RIGHT_READ, SCDK_OBJ_TASK, 0);
    if (status != SCDK_OK) {
        return status;
    }

    for (uint32_t i = 0; i < SCDK_MAX_USER_RINGS; i++) {
        if (user_rings[i].in_use) {
            continue;
        }

        scdk_cap_t ring = 0;
        status = scdk_ring_create(SCDK_BOOT_CORE, entries, 0, &ring);
        if (status != SCDK_OK) {
            return status;
        }

        user_rings[i].in_use = true;
        user_rings[i].source_task = source_task;
        user_rings[i].ring_cap = ring;
        user_rings[i].target_endpoint = 0;
        *out_ring = ring;

        scdk_log_write("ring", "user ring create pass");
        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_user_ring_bind(scdk_cap_t source_task,
                                  scdk_cap_t ring_cap,
                                  scdk_cap_t target_endpoint) {
    struct scdk_user_ring_slot *slot = 0;
    scdk_status_t status;

    if (target_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    status = user_ring_slot_for(source_task, ring_cap, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_ring_bind_target(ring_cap, target_endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    slot->target_endpoint = target_endpoint;
    return SCDK_OK;
}

scdk_status_t scdk_user_ring_submit(scdk_cap_t source_task,
                                    scdk_cap_t ring_cap,
                                    uintptr_t user_descs,
                                    uint32_t count) {
    struct scdk_user_ring_slot *slot = 0;
    struct scdk_message msg;
    scdk_status_t status;

    status = validate_count(count);
    if (status != SCDK_OK) {
        return status;
    }

    if (user_descs == 0u) {
        return SCDK_ERR_INVAL;
    }

    status = user_ring_slot_for(source_task, ring_cap, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    if (slot->target_endpoint == 0) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_user_validate_range(user_descs,
                                      sizeof(struct scdk_ring_desc) * count,
                                      false);
    if (status != SCDK_OK) {
        return status;
    }

    for (uint32_t i = 0; i < count; i++) {
        struct scdk_ring_desc desc;
        uintptr_t user_desc = user_descs +
                              ((uintptr_t)i * sizeof(struct scdk_ring_desc));

        status = scdk_user_copy_from(user_desc, &desc, sizeof(desc));
        if (status != SCDK_OK) {
            return status;
        }

        status = scdk_ring_submit(ring_cap, &desc);
        if (status != SCDK_OK) {
            return status;
        }
    }

    scdk_message_init(&msg, (uint64_t)source_task, 0, SCDK_MSG_RING_PROCESS);
    msg.arg0 = ring_cap;
    msg.arg1 = count;

    status = scdk_endpoint_call(slot->target_endpoint, &msg);
    if (status != SCDK_OK) {
        return status;
    }

    scdk_log_write("ring", "submit batch %u", count);
    return SCDK_OK;
}

scdk_status_t scdk_user_ring_poll(scdk_cap_t source_task,
                                  scdk_cap_t ring_cap,
                                  uintptr_t user_completions,
                                  uint32_t max_count,
                                  uint32_t *out_count) {
    struct scdk_user_ring_slot *slot = 0;
    uint32_t polled = 0;
    scdk_status_t status;

    status = validate_count(max_count);
    if (status != SCDK_OK) {
        return status;
    }

    if (user_completions == 0u || out_count == 0) {
        return SCDK_ERR_INVAL;
    }

    status = user_ring_slot_for(source_task, ring_cap, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    if (slot->target_endpoint == 0) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_user_validate_range(user_completions,
                                      sizeof(struct scdk_completion) * max_count,
                                      true);
    if (status != SCDK_OK) {
        return status;
    }

    while (polled < max_count) {
        struct scdk_completion completion;
        uintptr_t user_completion = user_completions +
                                    ((uintptr_t)polled * sizeof(struct scdk_completion));

        status = scdk_ring_poll(ring_cap, &completion);
        if (status == SCDK_ERR_NOENT) {
            break;
        }
        if (status != SCDK_OK) {
            return status;
        }

        status = scdk_user_copy_to(user_completion, &completion, sizeof(completion));
        if (status != SCDK_OK) {
            return status;
        }

        polled++;
    }

    *out_count = polled;
    if (polled == max_count) {
        scdk_log_write("ring", "completion batch %u pass", polled);
    }

    return SCDK_OK;
}
