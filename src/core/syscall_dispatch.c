// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/syscall.h>

#include <stdint.h>

#include <scdk/log.h>
#include <scdk/scheduler.h>
#include <scdk/task.h>
#include <scdk/user_grant.h>
#include <scdk/user_ipc.h>
#include <scdk/user_ring.h>

uint64_t scdk_syscall_return_value;

extern uint64_t scdk_syscall_kernel_stack_top;
extern uint64_t scdk_syscall_user_rsp;
extern uint64_t scdk_syscall_user_rip;
extern uint64_t scdk_syscall_user_rflags;
extern uint64_t scdk_syscall_arg0;
extern uint64_t scdk_syscall_arg1;
extern uint64_t scdk_syscall_arg2;
extern uint64_t scdk_syscall_arg3;
extern uint64_t scdk_user_return_rsp;
extern uint64_t scdk_user_return_rip;

static bool user_exited;
static bool endpoint_call_passed;

void scdk_syscall_reset_task_state(void) {
    user_exited = false;
    endpoint_call_passed = false;
    scdk_syscall_return_value = (uint64_t)SCDK_OK;
}

void scdk_syscall_save_task_state(struct scdk_syscall_task_state *out_state) {
    if (out_state == 0) {
        return;
    }

    out_state->kernel_stack_top = scdk_syscall_kernel_stack_top;
    out_state->user_rsp = scdk_syscall_user_rsp;
    out_state->user_rip = scdk_syscall_user_rip;
    out_state->user_rflags = scdk_syscall_user_rflags;
    out_state->arg0 = scdk_syscall_arg0;
    out_state->arg1 = scdk_syscall_arg1;
    out_state->arg2 = scdk_syscall_arg2;
    out_state->arg3 = scdk_syscall_arg3;
    out_state->return_value = scdk_syscall_return_value;
    out_state->user_return_rsp = scdk_user_return_rsp;
    out_state->user_return_rip = scdk_user_return_rip;
    out_state->user_exited = user_exited;
    out_state->endpoint_call_passed = endpoint_call_passed;
}

void scdk_syscall_restore_task_state(const struct scdk_syscall_task_state *state) {
    if (state == 0) {
        return;
    }

    scdk_syscall_kernel_stack_top = state->kernel_stack_top;
    scdk_syscall_user_rsp = state->user_rsp;
    scdk_syscall_user_rip = state->user_rip;
    scdk_syscall_user_rflags = state->user_rflags;
    scdk_syscall_arg0 = state->arg0;
    scdk_syscall_arg1 = state->arg1;
    scdk_syscall_arg2 = state->arg2;
    scdk_syscall_arg3 = state->arg3;
    scdk_syscall_return_value = state->return_value;
    scdk_user_return_rsp = state->user_return_rsp;
    scdk_user_return_rip = state->user_return_rip;
    user_exited = state->user_exited;
    endpoint_call_passed = state->endpoint_call_passed;
}

uint64_t scdk_syscall_dispatch(uint64_t number,
                               uint64_t arg0,
                               uint64_t arg1,
                               uint64_t arg2,
                               uint64_t arg3,
                               uint64_t user_rsp) {
    scdk_status_t status = SCDK_OK;

    (void)user_rsp;

    if (!scdk_syscall_ready()) {
        scdk_syscall_return_value = (uint64_t)SCDK_ERR_NOTSUP;
        return 1u;
    }

    switch (number) {
    case SCDK_SYS_DEBUG_WRITE:
        scdk_log_write("syscall", "debug call from user mode");
        break;
    case SCDK_SYS_ENDPOINT_CALL:
        status = scdk_sys_endpoint_call((scdk_cap_t)arg0, (uintptr_t)arg1);
        if (status == SCDK_OK) {
            endpoint_call_passed = true;
        }
        break;
    case SCDK_SYS_YIELD:
        scdk_yield();
        break;
    case SCDK_SYS_EXIT:
        user_exited = true;
        scdk_syscall_return_value = (uint64_t)SCDK_OK;
        return 1u;
    case SCDK_SYS_GRANT_CREATE: {
        scdk_cap_t task = 0;
        scdk_cap_t grant = 0;

        status = scdk_user_task_current(&task);
        if (status == SCDK_OK) {
            status = scdk_user_grant_create(task,
                                            (uintptr_t)arg0,
                                            arg1,
                                            arg2,
                                            (scdk_cap_t)arg3,
                                            &grant);
        }

        scdk_syscall_return_value = status == SCDK_OK ? grant : (uint64_t)status;
        return 0u;
    }
    case SCDK_SYS_GRANT_REVOKE:
        status = scdk_user_grant_revoke((scdk_cap_t)arg0);
        break;
    case SCDK_SYS_RING_CREATE: {
        scdk_cap_t task = 0;
        scdk_cap_t ring = 0;

        status = scdk_user_task_current(&task);
        if (status == SCDK_OK) {
            status = scdk_user_ring_create(task, (uint32_t)arg0, &ring);
        }

        scdk_syscall_return_value = status == SCDK_OK ? ring : (uint64_t)status;
        return 0u;
    }
    case SCDK_SYS_RING_BIND: {
        scdk_cap_t task = 0;

        status = scdk_user_task_current(&task);
        if (status == SCDK_OK) {
            status = scdk_user_ring_bind(task, (scdk_cap_t)arg0, (scdk_cap_t)arg1);
        }
        break;
    }
    case SCDK_SYS_RING_SUBMIT: {
        scdk_cap_t task = 0;

        status = scdk_user_task_current(&task);
        if (status == SCDK_OK) {
            status = scdk_user_ring_submit(task,
                                           (scdk_cap_t)arg0,
                                           (uintptr_t)arg1,
                                           (uint32_t)arg2);
        }
        if (status == SCDK_OK) {
            endpoint_call_passed = true;
        }
        break;
    }
    case SCDK_SYS_RING_POLL: {
        scdk_cap_t task = 0;
        uint32_t completed = 0;

        status = scdk_user_task_current(&task);
        if (status == SCDK_OK) {
            status = scdk_user_ring_poll(task,
                                         (scdk_cap_t)arg0,
                                         (uintptr_t)arg1,
                                         (uint32_t)arg2,
                                         &completed);
        }

        scdk_syscall_return_value = status == SCDK_OK ? completed : (uint64_t)status;
        return 0u;
    }
    default:
        scdk_log_error("unsupported syscall %llu",
                       (unsigned long long)number);
        status = SCDK_ERR_NOTSUP;
        user_exited = true;
        scdk_syscall_return_value = (uint64_t)status;
        return 1u;
    }

    scdk_syscall_return_value = (uint64_t)status;
    return 0u;
}

bool scdk_syscall_user_exited(void) {
    return user_exited;
}

bool scdk_syscall_endpoint_call_passed(void) {
    return endpoint_call_passed;
}
