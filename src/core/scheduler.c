// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/scheduler.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/heap.h>
#include <scdk/log.h>
#include <scdk/panic.h>

struct scdk_thread_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rsp;
};

struct scdk_task_slot {
    scdk_object_id_t object_id;
    scdk_cap_t cap;
    uint32_t owner_core;
    scdk_object_id_t address_space;
    scdk_cap_t main_thread;
};

struct scdk_thread_slot {
    struct scdk_thread_context context;
    scdk_object_id_t object_id;
    scdk_cap_t cap;
    scdk_cap_t task_cap;
    scdk_object_id_t task_id;
    scdk_thread_entry_t entry;
    void *arg;
    void *kernel_stack;
    uint64_t stack_top;
    uint32_t owner_core;
    uint32_t state;
    bool queued;
};

struct scdk_run_queue {
    scdk_cap_t entries[SCDK_MAX_THREADS];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
};

extern void scdk_context_switch(struct scdk_thread_context *old_context,
                                struct scdk_thread_context *new_context);

static struct scdk_task_slot task_table[SCDK_MAX_TASKS];
static struct scdk_thread_slot thread_table[SCDK_MAX_THREADS];
static struct scdk_run_queue run_queues[SCDK_MAX_CORES];
static struct scdk_thread_slot *current_thread_slot;
static struct scdk_thread_slot *scheduler_driver_slot;
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

static bool run_queue_empty(uint32_t owner_core) {
    return run_queues[owner_core].count == 0u;
}

static scdk_status_t enqueue_thread(struct scdk_thread_slot *slot) {
    struct scdk_run_queue *queue;

    if (slot == 0 || slot->owner_core >= SCDK_MAX_CORES) {
        return SCDK_ERR_INVAL;
    }

    if (slot->queued || slot->state != SCDK_THREAD_READY) {
        return SCDK_ERR_BUSY;
    }

    queue = &run_queues[slot->owner_core];
    if (queue->count >= SCDK_MAX_THREADS) {
        return SCDK_ERR_BUSY;
    }

    queue->entries[queue->tail] = slot->cap;
    queue->tail = (queue->tail + 1u) % SCDK_MAX_THREADS;
    queue->count++;
    slot->queued = true;
    return SCDK_OK;
}

static struct scdk_thread_slot *dequeue_thread(uint32_t owner_core) {
    struct scdk_run_queue *queue = &run_queues[owner_core];

    while (queue->count > 0u) {
        scdk_cap_t cap = queue->entries[queue->head];
        struct scdk_thread_slot *slot = 0;

        queue->head = (queue->head + 1u) % SCDK_MAX_THREADS;
        queue->count--;

        if (thread_slot_for_cap(cap, &slot) != SCDK_OK || slot == 0) {
            continue;
        }

        slot->queued = false;
        if (slot->state == SCDK_THREAD_READY) {
            return slot;
        }
    }

    return 0;
}

static void thread_trampoline(void);

static void make_kernel_thread_context(struct scdk_thread_slot *slot) {
    uintptr_t stack_top = (uintptr_t)slot->kernel_stack + SCDK_KERNEL_STACK_SIZE;
    uintptr_t sp = stack_top & ~(uintptr_t)0xfull;
    uint64_t *stack;

    sp -= 16u;
    stack = (uint64_t *)sp;
    stack[0] = (uint64_t)(uintptr_t)&thread_trampoline;
    stack[1] = 0;

    slot->context.r15 = 0;
    slot->context.r14 = 0;
    slot->context.r13 = 0;
    slot->context.r12 = 0;
    slot->context.rbx = 0;
    slot->context.rbp = 0;
    slot->context.rsp = sp;
    slot->stack_top = stack_top;
}

static void switch_to_thread(struct scdk_thread_slot *next) {
    struct scdk_thread_slot *old = current_thread_slot;

    if (next == 0 || old == 0 || next == old) {
        return;
    }

    next->state = SCDK_THREAD_RUNNING;
    current_thread_slot = next;
    current_thread = next->cap;
    current_task = next->task_cap;
    scdk_context_switch(&old->context, &next->context);
}

static void thread_trampoline(void) {
    struct scdk_thread_slot *slot = current_thread_slot;

    if (slot == 0 || slot->entry == 0) {
        scdk_panic("scheduler entered invalid kernel thread");
    }

    slot->entry(slot->arg);
    slot->state = SCDK_THREAD_DEAD;
    scdk_yield();
    scdk_panic("dead kernel thread resumed");
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
                                 SCDK_RIGHT_READ |
                                 SCDK_RIGHT_WRITE |
                                 SCDK_RIGHT_BIND,
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
                                 scdk_thread_entry_t entry,
                                 void *arg,
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
        thread_table[i].task_cap = task;
        thread_table[i].task_id = task_entry->object_id;
        thread_table[i].entry = entry;
        thread_table[i].arg = arg;
        thread_table[i].kernel_stack = 0;
        thread_table[i].stack_top = 0;
        thread_table[i].owner_core = task_slot->owner_core;
        thread_table[i].state = SCDK_THREAD_NEW;
        thread_table[i].queued = false;

        if (entry != 0) {
            thread_table[i].kernel_stack = scdk_kalloc(SCDK_KERNEL_STACK_SIZE);
            if (thread_table[i].kernel_stack == 0) {
                thread_table[i].object_id = 0;
                return SCDK_ERR_NOMEM;
            }
            make_kernel_thread_context(&thread_table[i]);
        }

        status = scdk_cap_create(object_id,
                                 SCDK_RIGHT_READ |
                                 SCDK_RIGHT_WRITE |
                                 SCDK_RIGHT_EXEC,
                                 out_thread);
        if (status != SCDK_OK) {
            scdk_kfree(thread_table[i].kernel_stack);
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

scdk_status_t scdk_thread_start(scdk_cap_t thread) {
    struct scdk_thread_slot *slot = 0;
    scdk_status_t status;

    status = scdk_cap_check(thread, SCDK_RIGHT_EXEC, SCDK_OBJ_THREAD, 0);
    if (status != SCDK_OK) {
        return status;
    }

    status = thread_slot_for_cap(thread, &slot);
    if (status != SCDK_OK) {
        return status;
    }

    if (slot->entry == 0 || slot->kernel_stack == 0) {
        return SCDK_ERR_INVAL;
    }

    if (slot->state != SCDK_THREAD_NEW) {
        return SCDK_ERR_BUSY;
    }

    slot->state = SCDK_THREAD_READY;
    status = enqueue_thread(slot);
    if (status != SCDK_OK) {
        slot->state = SCDK_THREAD_NEW;
    }

    return status;
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
    current_thread_slot = thread_slot;
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

void scdk_yield(void) {
    struct scdk_thread_slot *old = current_thread_slot;
    struct scdk_thread_slot *next;
    uint32_t owner_core;

    if (!scheduler_initialized || old == 0) {
        return;
    }

    owner_core = old->owner_core;
    next = dequeue_thread(owner_core);

    if (next == 0 &&
        scheduler_driver_slot != 0 &&
        old != scheduler_driver_slot &&
        scheduler_driver_slot->state == SCDK_THREAD_BLOCKED) {
        next = scheduler_driver_slot;
    }

    if (next == 0) {
        return;
    }

    if (old->state == SCDK_THREAD_RUNNING) {
        if (old == scheduler_driver_slot) {
            old->state = SCDK_THREAD_BLOCKED;
        } else {
            old->state = SCDK_THREAD_READY;
            if (enqueue_thread(old) != SCDK_OK) {
                old->state = SCDK_THREAD_RUNNING;
                return;
            }
        }
    }

    switch_to_thread(next);
}

void scdk_scheduler_run(void) {
    if (!scheduler_initialized || current_thread_slot == 0) {
        return;
    }

    scheduler_driver_slot = current_thread_slot;
    while (!run_queue_empty(scheduler_driver_slot->owner_core)) {
        scdk_yield();
    }

    if (scheduler_driver_slot->state == SCDK_THREAD_BLOCKED) {
        scheduler_driver_slot->state = SCDK_THREAD_RUNNING;
    }
    scheduler_driver_slot = 0;
}

scdk_status_t scdk_scheduler_yield_stub(void) {
    if (!scheduler_initialized || current_task == 0 || current_thread == 0) {
        return SCDK_ERR_NOTSUP;
    }

    scdk_yield();
    return SCDK_OK;
}
