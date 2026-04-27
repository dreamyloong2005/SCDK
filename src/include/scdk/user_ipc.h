// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_USER_IPC_H
#define SCDK_USER_IPC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_USER_IPC_MAX_WRITE 256u
#define SCDK_USER_IPC_MAX_PATH 128u

/*
 * Control-plane: validate a current-user-address-space range without copying.
 */
scdk_status_t scdk_user_validate_range(uintptr_t user_ptr,
                                       size_t size,
                                       bool writable);

/*
 * Control-plane: validate and copy bytes from the current user address space.
 * Requires every touched page to be mapped with SCDK_VMM_MAP_USER.
 */
scdk_status_t scdk_user_copy_from(uintptr_t user_ptr,
                                  void *kernel_dst,
                                  size_t size);

/*
 * Control-plane: validate and copy bytes to the current user address space.
 * Requires every touched page to be user-accessible and writable.
 */
scdk_status_t scdk_user_copy_to(uintptr_t user_ptr,
                                const void *kernel_src,
                                size_t size);

/*
 * Control-plane syscall helper: deliver a user message to an endpoint.
 * Requires endpoint_cap to carry SCDK_RIGHT_SEND for SCDK_OBJ_ENDPOINT.
 */
scdk_status_t scdk_sys_endpoint_call(scdk_cap_t endpoint_cap,
                                     uintptr_t user_message_ptr);

#endif
