// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_VFS_H
#define SCDK_VFS_H

#include <scdk/capability.h>
#include <scdk/types.h>

#define SCDK_VFS_MAX_NAME 64u

enum scdk_vfs_node_type {
    SCDK_VFS_NODE_NONE = 0,
    SCDK_VFS_NODE_FILE = 1,
    SCDK_VFS_NODE_DIR = 2,
};

struct scdk_vfs_stat {
    uint64_t type;
    uint64_t size;
};

struct scdk_vfs_dirent {
    char name[SCDK_VFS_MAX_NAME];
    uint64_t type;
    uint64_t size;
};

/*
 * Control-plane: start the SCDK-native VFS router and mount tmpfs at /.
 * The VFS exposes endpoint/message file operations and does not create POSIX
 * file descriptors.
 */
scdk_status_t scdk_vfs_service_init(scdk_cap_t *out_endpoint);

#endif
