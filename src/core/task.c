// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/task.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/address_space.h>
#include <scdk/capability.h>
#include <scdk/log.h>
#include <scdk/service.h>
#include <scdk/thread.h>
#include <scdk/usermode.h>

struct scdk_user_task_slot {
    bool in_use;
    scdk_cap_t task_cap;
    scdk_task_id_t task_id;
    scdk_cap_t address_space_cap;
    scdk_object_id_t address_space_id;
    scdk_cap_t main_thread_cap;
    scdk_object_id_t capability_table_placeholder;
    uint32_t owner_core;
    uint32_t state;
    int exit_status;
};

struct scdk_user_thread_slot {
    bool in_use;
    scdk_cap_t thread_cap;
    scdk_thread_id_t thread_id;
    scdk_task_id_t task_id;
    uint64_t user_stack_top;
    uint64_t instruction_pointer;
    uint32_t owner_core;
    uint32_t state;
};

static struct scdk_user_task_slot user_tasks[SCDK_MAX_TASKS];
static struct scdk_user_thread_slot user_threads[SCDK_MAX_THREADS];
static scdk_cap_t current_user_task;

static scdk_status_t object_id_from_cap(scdk_cap_t cap,
                                        uint32_t expected_type,
                                        scdk_object_id_t *out_id) {
    const struct scdk_cap_entry *entry = 0;
    scdk_status_t status;

    if (out_id == 0) {
        return SCDK_ERR_INVAL;
    }

    status = scdk_cap_check(cap, SCDK_RIGHT_READ, expected_type, &entry);
    if (status != SCDK_OK) {
        return status;
    }

    *out_id = entry->object_id;
    return SCDK_OK;
}

static scdk_status_t task_slot_for_cap(scdk_cap_t task,
                                       struct scdk_user_task_slot **out_slot) {
    if (out_slot == 0) {
        return SCDK_ERR_INVAL;
    }

    for (uint32_t i = 0; i < SCDK_MAX_TASKS; i++) {
        if (user_tasks[i].in_use && user_tasks[i].task_cap == task) {
            *out_slot = &user_tasks[i];
            return SCDK_OK;
        }
    }

    return SCDK_ERR_NOENT;
}

static scdk_status_t thread_slot_for_cap(scdk_cap_t thread,
                                         struct scdk_user_thread_slot **out_slot) {
    if (out_slot == 0) {
        return SCDK_ERR_INVAL;
    }

    for (uint32_t i = 0; i < SCDK_MAX_THREADS; i++) {
        if (user_threads[i].in_use && user_threads[i].thread_cap == thread) {
            *out_slot = &user_threads[i];
            return SCDK_OK;
        }
    }

    return SCDK_ERR_NOENT;
}

static struct scdk_user_task_slot *alloc_task_slot(void) {
    for (uint32_t i = 0; i < SCDK_MAX_TASKS; i++) {
        if (!user_tasks[i].in_use) {
            return &user_tasks[i];
        }
    }

    return 0;
}

static struct scdk_user_thread_slot *alloc_thread_slot(void) {
    for (uint32_t i = 0; i < SCDK_MAX_THREADS; i++) {
        if (!user_threads[i].in_use) {
            return &user_threads[i];
        }
    }

    return 0;
}

scdk_status_t scdk_user_thread_create(scdk_cap_t task,
                                      scdk_cap_t *out_thread) {
    struct scdk_user_task_slot *task_slot = 0;
    struct scdk_user_thread_slot *thread_slot;
    scdk_cap_t thread = 0;
    scdk_thread_id_t thread_id = 0;
    scdk_status_t status;

    if (out_thread == 0) {
        return SCDK_ERR_INVAL;
    }

    status = task_slot_for_cap(task, &task_slot);
    if (status != SCDK_OK) {
        return status;
    }

    thread_slot = alloc_thread_slot();
    if (thread_slot == 0) {
        return SCDK_ERR_NOMEM;
    }

    status = scdk_thread_create(task, 0, 0, &thread);
    if (status != SCDK_OK) {
        return status;
    }

    status = object_id_from_cap(thread, SCDK_OBJ_THREAD, &thread_id);
    if (status != SCDK_OK) {
        return status;
    }

    thread_slot->in_use = true;
    thread_slot->thread_cap = thread;
    thread_slot->thread_id = thread_id;
    thread_slot->task_id = task_slot->task_id;
    thread_slot->user_stack_top = SCDK_USER_TEST_STACK_TOP;
    thread_slot->instruction_pointer = SCDK_USER_TEST_CODE_VIRT;
    thread_slot->owner_core = task_slot->owner_core;
    thread_slot->state = SCDK_THREAD_NEW;

    task_slot->main_thread_cap = thread;
    *out_thread = thread;
    return SCDK_OK;
}

scdk_status_t scdk_user_task_create(scdk_cap_t *out_task,
                                    scdk_cap_t *out_address_space,
                                    scdk_cap_t *out_main_thread) {
    struct scdk_user_task_slot *slot;
    scdk_cap_t aspace = 0;
    scdk_cap_t task = 0;
    scdk_cap_t main_thread = 0;
    scdk_object_id_t aspace_id = 0;
    scdk_task_id_t task_id = 0;
    scdk_status_t status;

    if (out_task == 0 || out_address_space == 0 || out_main_thread == 0) {
        return SCDK_ERR_INVAL;
    }

    slot = alloc_task_slot();
    if (slot == 0) {
        return SCDK_ERR_NOMEM;
    }

    status = scdk_address_space_create(&aspace);
    if (status != SCDK_OK) {
        return status;
    }

    status = object_id_from_cap(aspace, SCDK_OBJ_ADDRESS_SPACE, &aspace_id);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_task_create(SCDK_BOOT_CORE, aspace_id, &task);
    if (status != SCDK_OK) {
        return status;
    }

    status = object_id_from_cap(task, SCDK_OBJ_TASK, &task_id);
    if (status != SCDK_OK) {
        return status;
    }

    slot->in_use = true;
    slot->task_cap = task;
    slot->task_id = task_id;
    slot->address_space_cap = aspace;
    slot->address_space_id = aspace_id;
    slot->main_thread_cap = 0;
    slot->capability_table_placeholder = 0;
    slot->owner_core = SCDK_BOOT_CORE;
    slot->state = SCDK_TASK_NEW;
    slot->exit_status = 0;

    status = scdk_user_thread_create(task, &main_thread);
    if (status != SCDK_OK) {
        slot->in_use = false;
        return status;
    }

    *out_task = task;
    *out_address_space = aspace;
    *out_main_thread = main_thread;

    scdk_log_write("task", "user task created");
    return SCDK_OK;
}

scdk_status_t scdk_user_task_run_builtin(scdk_cap_t task,
                                         uint64_t hhdm_offset) {
    struct scdk_user_task_slot *task_slot = 0;
    struct scdk_user_thread_slot *thread_slot = 0;
    scdk_cap_t old_user_task;
    scdk_status_t status;

    status = task_slot_for_cap(task, &task_slot);
    if (status != SCDK_OK) {
        return status;
    }

    status = thread_slot_for_cap(task_slot->main_thread_cap, &thread_slot);
    if (status != SCDK_OK) {
        return status;
    }

    if (task_slot->state != SCDK_TASK_NEW ||
        thread_slot->state != SCDK_THREAD_NEW) {
        return SCDK_ERR_BUSY;
    }

    task_slot->state = SCDK_TASK_RUNNING;
    thread_slot->state = SCDK_THREAD_RUNNING;
    scdk_log_write("task", "main thread started");

    old_user_task = current_user_task;
    current_user_task = task;
    status = scdk_usermode_run_task_test(task_slot->address_space_cap,
                                         thread_slot->thread_cap,
                                         hhdm_offset);
    current_user_task = old_user_task;
    if (status != SCDK_OK) {
        return status;
    }

    return scdk_task_exit(task, 0);
}

scdk_status_t scdk_user_task_run_flat(scdk_cap_t task,
                                      const void *image,
                                      size_t image_size,
                                      scdk_cap_t bootstrap_endpoint,
                                      uint64_t hhdm_offset) {
    struct scdk_user_task_slot *task_slot = 0;
    struct scdk_user_thread_slot *thread_slot = 0;
    scdk_cap_t old_user_task;
    scdk_status_t status;

    if (image == 0 || image_size == 0u) {
        return SCDK_ERR_INVAL;
    }

    status = task_slot_for_cap(task, &task_slot);
    if (status != SCDK_OK) {
        return status;
    }

    status = thread_slot_for_cap(task_slot->main_thread_cap, &thread_slot);
    if (status != SCDK_OK) {
        return status;
    }

    if (task_slot->state != SCDK_TASK_NEW ||
        thread_slot->state != SCDK_THREAD_NEW) {
        return SCDK_ERR_BUSY;
    }

    task_slot->state = SCDK_TASK_RUNNING;
    thread_slot->state = SCDK_THREAD_RUNNING;
    scdk_log_write("task", "main thread started");

    old_user_task = current_user_task;
    current_user_task = task;
    status = scdk_usermode_run_flat_image(task_slot->address_space_cap,
                                          thread_slot->thread_cap,
                                          thread_slot->instruction_pointer,
                                          image,
                                          image_size,
                                          bootstrap_endpoint,
                                          hhdm_offset);
    current_user_task = old_user_task;
    if (status != SCDK_OK) {
        task_slot->state = SCDK_TASK_DEAD;
        thread_slot->state = SCDK_THREAD_DEAD;
        return status;
    }

    return scdk_task_exit(task, 0);
}

scdk_status_t scdk_user_task_run_fault_test(scdk_cap_t task,
                                            enum scdk_fault_user_test test,
                                            uint64_t hhdm_offset) {
    struct scdk_user_task_slot *task_slot = 0;
    struct scdk_user_thread_slot *thread_slot = 0;
    scdk_cap_t bootstrap_endpoint = 0;
    scdk_cap_t old_user_task;
    scdk_status_t status;

    status = task_slot_for_cap(task, &task_slot);
    if (status != SCDK_OK) {
        return status;
    }

    status = thread_slot_for_cap(task_slot->main_thread_cap, &thread_slot);
    if (status != SCDK_OK) {
        return status;
    }

    if (task_slot->state != SCDK_TASK_NEW ||
        thread_slot->state != SCDK_THREAD_NEW) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_service_lookup(SCDK_SERVICE_CONSOLE, &bootstrap_endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    task_slot->state = SCDK_TASK_RUNNING;
    thread_slot->state = SCDK_THREAD_RUNNING;
    scdk_log_write("task", "main thread started");

    old_user_task = current_user_task;
    current_user_task = task;
    status = scdk_usermode_run_fault_test(task_slot->address_space_cap,
                                          thread_slot->thread_cap,
                                          test,
                                          bootstrap_endpoint,
                                          hhdm_offset);
    current_user_task = old_user_task;
    if (status != SCDK_OK) {
        task_slot->state = SCDK_TASK_DEAD;
        thread_slot->state = SCDK_THREAD_DEAD;
        return status;
    }

    if (task_slot->state != SCDK_TASK_DEAD) {
        return SCDK_ERR_BUSY;
    }

    return SCDK_OK;
}

scdk_status_t scdk_task_exit(scdk_cap_t task,
                             int status_code) {
    struct scdk_user_task_slot *task_slot = 0;
    struct scdk_user_thread_slot *thread_slot = 0;
    scdk_status_t status = task_slot_for_cap(task, &task_slot);

    if (status != SCDK_OK) {
        return status;
    }

    if (task_slot->state != SCDK_TASK_RUNNING) {
        return SCDK_ERR_BUSY;
    }

    task_slot->exit_status = status_code;
    task_slot->state = SCDK_TASK_DEAD;

    if (thread_slot_for_cap(task_slot->main_thread_cap, &thread_slot) == SCDK_OK) {
        thread_slot->state = SCDK_THREAD_DEAD;
    }

    scdk_log_write("task", "user task exited");
    return SCDK_OK;
}

scdk_status_t scdk_task_fault_current(int status_code) {
    struct scdk_user_task_slot *task_slot = 0;
    struct scdk_user_thread_slot *thread_slot = 0;
    scdk_status_t status;

    if (current_user_task == 0) {
        return SCDK_ERR_NOENT;
    }

    status = task_slot_for_cap(current_user_task, &task_slot);
    if (status != SCDK_OK) {
        return status;
    }

    task_slot->exit_status = status_code;
    task_slot->state = SCDK_TASK_DEAD;

    if (thread_slot_for_cap(task_slot->main_thread_cap, &thread_slot) == SCDK_OK) {
        thread_slot->state = SCDK_THREAD_DEAD;
    }

    scdk_log_write("fault", "task killed");
    return SCDK_OK;
}

scdk_status_t scdk_task_cleanup(scdk_cap_t task) {
    struct scdk_user_task_slot *task_slot = 0;
    struct scdk_user_thread_slot *thread_slot = 0;
    scdk_status_t status = task_slot_for_cap(task, &task_slot);

    if (status != SCDK_OK) {
        return status;
    }

    if (task_slot->state != SCDK_TASK_DEAD) {
        return SCDK_ERR_BUSY;
    }

    if (thread_slot_for_cap(task_slot->main_thread_cap, &thread_slot) == SCDK_OK) {
        thread_slot->state = SCDK_THREAD_DEAD;
    }

    task_slot->state = SCDK_TASK_CLEANED;
    scdk_log_write("task", "cleanup pass");
    return SCDK_OK;
}

scdk_status_t scdk_user_task_state(scdk_cap_t task,
                                   uint32_t *out_state) {
    struct scdk_user_task_slot *slot = 0;
    scdk_status_t status;

    if (out_state == 0) {
        return SCDK_ERR_INVAL;
    }

    status = task_slot_for_cap(task, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    *out_state = slot->state;
    return SCDK_OK;
}

scdk_status_t scdk_user_thread_state(scdk_cap_t thread,
                                     uint32_t *out_state) {
    struct scdk_user_thread_slot *slot = 0;
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

scdk_status_t scdk_user_task_current(scdk_cap_t *out_task) {
    if (out_task == 0) {
        return SCDK_ERR_INVAL;
    }

    if (current_user_task == 0) {
        return SCDK_ERR_NOENT;
    }

    *out_task = current_user_task;
    return SCDK_OK;
}
