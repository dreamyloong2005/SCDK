// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/syscall.h>

#include <stdint.h>

#include <scdk/log.h>
#include <scdk/scheduler.h>
#include <scdk/user_ipc.h>

uint64_t scdk_syscall_return_value;

static bool user_exited;
static bool endpoint_call_passed;

void scdk_syscall_reset_task_state(void) {
    user_exited = false;
    endpoint_call_passed = false;
    scdk_syscall_return_value = (uint64_t)SCDK_OK;
}

uint64_t scdk_syscall_dispatch(uint64_t number,
                               uint64_t arg0,
                               uint64_t arg1,
                               uint64_t arg2,
                               uint64_t arg3,
                               uint64_t user_rsp) {
    scdk_status_t status = SCDK_OK;

    (void)arg2;
    (void)arg3;
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
