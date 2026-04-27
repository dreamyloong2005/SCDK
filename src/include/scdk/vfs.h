// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_VFS_H
#define SCDK_VFS_H

#include <scdk/capability.h>
#include <scdk/types.h>

/*
 * Control-plane: start the SCDK-native VFS router and mount tmpfs at /.
 * The VFS exposes endpoint/message file operations and does not create POSIX
 * file descriptors.
 */
scdk_status_t scdk_vfs_service_init(scdk_cap_t *out_endpoint);

#endif
