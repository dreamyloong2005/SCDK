// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/scheduler.h>

#include <stdbool.h>

#include <scdk/log.h>

struct scdk_task_slot {
    scdk_object_id_t object_id;
    scdk_cap_t cap;
    uint32_t owner_core;
    scdk_object_id_t address_space;
    scdk_cap_t main_thread;
};

struct scdk_thread_slot {
    scdk_object_id_t object_id;
    scdk_cap_t cap;
    scdk_object_id_t task_id;
    uint64_t entry;
    uint64_t stack_top;
    uint32_t owner_core;
    uint32_t state;
};

static struct scdk_task_slot task_table[SCDK_MAX_TASKS];
static struct scdk_thread_slot thread_table[SCDK_MAX_THREADS];
static scdk_cap_t current_task;
static scdk_cap_t current_thread;
static bool scheduler_initialized;

static scdk_status_t task_slot_for_cap(scdk_cap_t task,
                                       struct scdk_task_slot **out_slot) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_slot == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(task,
                            SCDK_RIGHT_READ,
                            SCDK_OBJ_TASK,
                            &entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    struct scdk_task_slot *slot = object->payload;
    if (slot == 0 || slot->object_id != entry->object_id) {
        return SCDK_ERR_NOENT;
    }

    *out_slot = slot;
    return SCDK_OK;
}

static scdk_status_t thread_slot_for_cap(scdk_cap_t thread,
                                         struct scdk_thread_slot **out_slot) {
    const struct scdk_cap_entry *entry = 0;
    const struct scdk_object *object = 0;
    scdk_status_t status;

    if (out_slot == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(thread,
                            SCDK_RIGHT_READ,
                            SCDK_OBJ_THREAD,
                            &entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_object_lookup(entry->object_id, &object);
    if (status != SCDK_OK) {
        return status;
    }

    struct scdk_thread_slot *slot = object->payload;
    if (slot == 0 || slot->object_id != entry->object_id) {
        return SCDK_ERR_NOENT;
    }

    *out_slot = slot;
    return SCDK_OK;
}

scdk_status_t scdk_task_create(uint32_t owner_core,
                               scdk_object_id_t address_space,
                               scdk_cap_t *out_task) {
    if (out_task == 0 || owner_core >= SCDK_MAX_CORES) {
        return SCDK_ERR_INVAL;
    }

    for (uint32_t i = 0; i < SCDK_MAX_TASKS; i++) {
        if (task_table[i].object_id != 0) {
            continue;
        }

        scdk_object_id_t object_id = 0;
        scdk_status_t status = scdk_object_create(SCDK_OBJ_TASK,
                                                  owner_core,
                                                  0,
                                                  &task_table[i],
                                                  &object_id);
        if (status != SCDK_OK) {
            return status;
        }

        task_table[i].object_id = object_id;
        task_table[i].cap = 0;
        task_table[i].owner_core = owner_core;
        task_table[i].address_space = address_space;
        task_table[i].main_thread = 0;

        status = scdk_cap_create(object_id,
                                 SCDK_RIGHT_READ | SCDK_RIGHT_WRITE | SCDK_RIGHT_BIND,
                                 out_task);
        if (status != SCDK_OK) {
            task_table[i].object_id = 0;
            return status;
        }

        task_table[i].cap = *out_task;
        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_thread_create(scdk_cap_t task,
                                 uint64_t entry,
                                 uint64_t stack_top,
                                 scdk_cap_t *out_thread) {
    const struct scdk_cap_entry *task_entry = 0;
    struct scdk_task_slot *task_slot = 0;
    scdk_status_t status;

    if (out_thread == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(task,
                            SCDK_RIGHT_BIND,
                            SCDK_OBJ_TASK,
                            &task_entry);
    if (status != SCDK_OK) {
        return status;
    }

    status = task_slot_for_cap(task, &task_slot);
    if (status != SCDK_OK) {
        return status;
    }

    for (uint32_t i = 0; i < SCDK_MAX_THREADS; i++) {
        if (thread_table[i].object_id != 0) {
            continue;
        }

        scdk_object_id_t object_id = 0;
        status = scdk_object_create(SCDK_OBJ_THREAD,
                                    task_slot->owner_core,
                                    0,
                                    &thread_table[i],
                                    &object_id);
        if (status != SCDK_OK) {
            return status;
        }

        thread_table[i].object_id = object_id;
        thread_table[i].cap = 0;
        thread_table[i].task_id = task_entry->object_id;
        thread_table[i].entry = entry;
        thread_table[i].stack_top = stack_top;
        thread_table[i].owner_core = task_slot->owner_core;
        thread_table[i].state = SCDK_THREAD_READY;

        status = scdk_cap_create(object_id,
                                 SCDK_RIGHT_READ | SCDK_RIGHT_WRITE | SCDK_RIGHT_EXEC,
                                 out_thread);
        if (status != SCDK_OK) {
            thread_table[i].object_id = 0;
            return status;
        }

        thread_table[i].cap = *out_thread;
        if (task_slot->main_thread == 0) {
            task_slot->main_thread = *out_thread;
        }

        return SCDK_OK;
    }

    return SCDK_ERR_NOMEM;
}

scdk_status_t scdk_scheduler_init(scdk_cap_t *out_boot_task,
                                  scdk_cap_t *out_boot_thread) {
    scdk_cap_t boot_task = 0;
    scdk_cap_t boot_thread = 0;
    struct scdk_thread_slot *thread_slot = 0;
    scdk_status_t status;

    if (scheduler_initialized) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_task_create(SCDK_BOOT_CORE, 0, &boot_task);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_thread_create(boot_task, 0, 0, &boot_thread);
    if (status != SCDK_OK) {
        return status;
    }

    status = thread_slot_for_cap(boot_thread, &thread_slot);
    if (status != SCDK_OK) {
        return status;
    }

    thread_slot->state = SCDK_THREAD_RUNNING;
    current_task = boot_task;
    current_thread = boot_thread;
    scheduler_initialized = true;

    if (out_boot_task != 0) {
        *out_boot_task = boot_task;
    }

    if (out_boot_thread != 0) {
        *out_boot_thread = boot_thread;
    }

    scdk_log_info("scheduler boot task/thread created");
    return SCDK_OK;
}

scdk_status_t scdk_scheduler_current_task(scdk_cap_t *out_task) {
    if (out_task == 0) {
        return SCDK_ERR_INVAL;
    }

    if (!scheduler_initialized || current_task == 0) {
        return SCDK_ERR_NOTSUP;
    }

    *out_task = current_task;
    return SCDK_OK;
}

scdk_status_t scdk_scheduler_current_thread(scdk_cap_t *out_thread) {
    if (out_thread == 0) {
        return SCDK_ERR_INVAL;
    }

    if (!scheduler_initialized || current_thread == 0) {
        return SCDK_ERR_NOTSUP;
    }

    *out_thread = current_thread;
    return SCDK_OK;
}

scdk_status_t scdk_task_main_thread(scdk_cap_t task,
                                    scdk_cap_t *out_thread) {
    struct scdk_task_slot *slot = 0;
    scdk_status_t status;

    if (out_thread == 0) {
        return SCDK_ERR_INVAL;
    }

    status = task_slot_for_cap(task, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    if (slot->main_thread == 0) {
        return SCDK_ERR_NOENT;
    }

    *out_thread = slot->main_thread;
    return SCDK_OK;
}

scdk_status_t scdk_thread_task_id(scdk_cap_t thread,
                                  scdk_object_id_t *out_task_id) {
    struct scdk_thread_slot *slot = 0;
    scdk_status_t status;

    if (out_task_id == 0) {
        return SCDK_ERR_INVAL;
    }

    status = thread_slot_for_cap(thread, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    *out_task_id = slot->task_id;
    return SCDK_OK;
}

scdk_status_t scdk_thread_state(scdk_cap_t thread,
                                uint32_t *out_state) {
    struct scdk_thread_slot *slot = 0;
    scdk_status_t status;

    if (out_state == 0) {
        return SCDK_ERR_INVAL;
    }

    status = thread_slot_for_cap(thread, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    *out_state = slot->state;
    return SCDK_OK;
}

scdk_status_t scdk_scheduler_yield_stub(void) {
    if (!scheduler_initialized || current_task == 0 || current_thread == 0) {
        return SCDK_ERR_NOTSUP;
    }

    return SCDK_OK;
}
