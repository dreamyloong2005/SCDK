// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_LOADER_H
#define SCDK_LOADER_H

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_LOADER_FLAT_ENTRY_VIRT 0x0000000000400000ull
#define SCDK_LOADER_MAX_IMAGE_SIZE  65536u

/*
 * Control-plane: load a flat user executable through the SCDK VFS service,
 * create a user task/address space/thread, and start it at the fixed flat
 * entry address. The file path is resolved through endpoint/message calls.
 */
scdk_status_t scdk_loader_load_from_vfs(const char *path,
                                        uint64_t hhdm_offset,
                                        scdk_cap_t *out_task,
                                        scdk_cap_t *out_main_thread);

/*
 * Control-plane: load a flat executable and pass a caller-selected bootstrap
 * endpoint capability in the new task's first user argument.
 */
scdk_status_t scdk_loader_load_from_vfs_with_endpoint(const char *path,
                                                      uint64_t hhdm_offset,
                                                      scdk_cap_t bootstrap_endpoint,
                                                      scdk_cap_t *out_task,
                                                      scdk_cap_t *out_main_thread);

#endif
