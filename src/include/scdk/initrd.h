// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_INITRD_H
#define SCDK_INITRD_H

#include <stdint.h>

#include <limine.h>

#include <scdk/types.h>
#include <scdk/vfs.h>

#define SCDK_INITRD_MAX_FILES 32u
#define SCDK_INITRD_MAX_PATH 128u

struct scdk_initrd_file {
    const char *path;
    const void *data;
    uint64_t size;
    uint32_t flags;
};

/*
 * Boot control-plane: provide the Limine module response before initrd init.
 */
void scdk_initrd_set_limine_response(const struct limine_module_response *response);

/*
 * Control-plane: find and parse the boot initrd module.
 */
scdk_status_t scdk_initrd_init_from_limine(void);

/*
 * Control-plane diagnostic: print the parsed initrd file table.
 */
scdk_status_t scdk_initrd_list(void);

/*
 * Control-plane: find one initrd file by absolute SCDK path.
 */
scdk_status_t scdk_initrd_find(const char *path,
                               struct scdk_initrd_file *out_file);

/*
 * Control-plane: return file or synthetic directory metadata for the parsed
 * initrd file table.
 */
scdk_status_t scdk_initrd_stat(const char *path,
                               struct scdk_vfs_stat *out_stat);

/*
 * Control-plane: list one synthetic initrd directory. Directories are derived
 * from slash-separated file paths inside the ustar payload.
 */
scdk_status_t scdk_initrd_listdir(const char *path,
                                  struct scdk_vfs_dirent *entries,
                                  uint64_t capacity,
                                  uint64_t *out_count);

#endif
