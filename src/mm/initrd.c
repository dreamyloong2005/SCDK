// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/initrd.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <scdk/log.h>
#include <scdk/string.h>

#define SCDK_TAR_BLOCK_SIZE 512ull
#define SCDK_TAR_TYPE_FILE '0'
#define SCDK_TAR_TYPE_OLD_FILE '\0'

struct scdk_tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} __attribute__((packed));

struct scdk_initrd_entry {
    char path[SCDK_INITRD_MAX_PATH];
    const uint8_t *data;
    uint64_t size;
    uint32_t flags;
};

static const struct limine_module_response *limine_modules;
static struct scdk_initrd_entry initrd_files[SCDK_INITRD_MAX_FILES];
static uint32_t initrd_file_count;
static bool initrd_initialized;

void scdk_initrd_set_limine_response(const struct limine_module_response *response) {
    limine_modules = response;
}

static bool memory_is_zero(const void *ptr, size_t size) {
    const uint8_t *bytes = ptr;

    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != 0u) {
            return false;
        }
    }

    return true;
}

static bool str_eq(const char *a, const char *b) {
    if (a == 0 || b == 0) {
        return false;
    }

    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static bool str_ends_with(const char *value, const char *suffix) {
    size_t value_len;
    size_t suffix_len;

    if (value == 0 || suffix == 0) {
        return false;
    }

    value_len = strlen(value);
    suffix_len = strlen(suffix);
    if (suffix_len > value_len) {
        return false;
    }

    return memcmp(value + value_len - suffix_len, suffix, suffix_len) == 0;
}

static scdk_status_t parse_octal(const char *field,
                                 size_t field_size,
                                 uint64_t *out_value) {
    uint64_t value = 0;
    bool saw_digit = false;

    if (field == 0 || out_value == 0) {
        return SCDK_ERR_INVAL;
    }

    for (size_t i = 0; i < field_size; i++) {
        char c = field[i];

        if (c == '\0' || c == ' ') {
            break;
        }

        if (c < '0' || c > '7') {
            return SCDK_ERR_INVAL;
        }

        saw_digit = true;
        value = (value << 3u) | (uint64_t)(c - '0');
    }

    if (!saw_digit) {
        return SCDK_ERR_INVAL;
    }

    *out_value = value;
    return SCDK_OK;
}

static size_t bounded_cstr_len(const char *s, size_t max) {
    size_t len = 0;

    while (len < max && s[len] != '\0') {
        len++;
    }

    return len;
}

static scdk_status_t append_path_component(char *out,
                                           size_t out_size,
                                           size_t *inout_pos,
                                           const char *component,
                                           size_t component_max) {
    size_t component_len;

    if (out == 0 || inout_pos == 0 || component == 0 || out_size == 0u) {
        return SCDK_ERR_INVAL;
    }

    component_len = bounded_cstr_len(component, component_max);
    if (component_len == 0u) {
        return SCDK_OK;
    }

    if (*inout_pos + component_len >= out_size) {
        return SCDK_ERR_BOUNDS;
    }

    memcpy(out + *inout_pos, component, component_len);
    *inout_pos += component_len;
    out[*inout_pos] = '\0';
    return SCDK_OK;
}

static scdk_status_t normalize_tar_path(const struct scdk_tar_header *header,
                                        char *out,
                                        size_t out_size) {
    size_t pos = 0;
    scdk_status_t status;

    if (header == 0 || out == 0 || out_size < 2u) {
        return SCDK_ERR_INVAL;
    }

    out[pos++] = '/';
    out[pos] = '\0';

    status = append_path_component(out,
                                   out_size,
                                   &pos,
                                   header->prefix,
                                   sizeof(header->prefix));
    if (status != SCDK_OK) {
        return status;
    }

    if (pos > 1u) {
        if (pos + 1u >= out_size) {
            return SCDK_ERR_BOUNDS;
        }
        out[pos++] = '/';
        out[pos] = '\0';
    }

    if (header->name[0] == '.' && header->name[1] == '/') {
        status = append_path_component(out,
                                       out_size,
                                       &pos,
                                       header->name + 2,
                                       sizeof(header->name) - 2u);
    } else {
        status = append_path_component(out,
                                       out_size,
                                       &pos,
                                       header->name,
                                       sizeof(header->name));
    }

    if (status != SCDK_OK) {
        return status;
    }

    if (pos <= 1u) {
        return SCDK_ERR_NOENT;
    }

    return SCDK_OK;
}

static bool tar_header_is_file(const struct scdk_tar_header *header) {
    return header->typeflag == SCDK_TAR_TYPE_FILE ||
           header->typeflag == SCDK_TAR_TYPE_OLD_FILE;
}

static scdk_status_t remember_file(const char *path,
                                   const uint8_t *data,
                                   uint64_t size) {
    struct scdk_initrd_entry *entry;

    if (path == 0 || data == 0 || initrd_file_count >= SCDK_INITRD_MAX_FILES) {
        return SCDK_ERR_NOMEM;
    }

    entry = &initrd_files[initrd_file_count++];
    memcpy(entry->path, path, strlen(path) + 1u);
    entry->data = data;
    entry->size = size;
    entry->flags = 0;
    return SCDK_OK;
}

static scdk_status_t parse_tar_image(const uint8_t *base, uint64_t size) {
    uint64_t offset = 0;

    if (base == 0 || size < SCDK_TAR_BLOCK_SIZE) {
        return SCDK_ERR_INVAL;
    }

    initrd_file_count = 0;
    memset(initrd_files, 0, sizeof(initrd_files));

    while (offset + SCDK_TAR_BLOCK_SIZE <= size) {
        const struct scdk_tar_header *header =
            (const struct scdk_tar_header *)(const void *)(base + offset);
        uint64_t file_size = 0;
        uint64_t data_offset = offset + SCDK_TAR_BLOCK_SIZE;
        uint64_t padded_size;
        char path[SCDK_INITRD_MAX_PATH];
        scdk_status_t status;

        if (memory_is_zero(header, sizeof(*header))) {
            break;
        }

        status = parse_octal(header->size, sizeof(header->size), &file_size);
        if (status != SCDK_OK) {
            return status;
        }

        padded_size = (file_size + SCDK_TAR_BLOCK_SIZE - 1u) &
                      ~(SCDK_TAR_BLOCK_SIZE - 1u);
        if (data_offset > size || padded_size > size - data_offset) {
            return SCDK_ERR_BOUNDS;
        }

        if (tar_header_is_file(header)) {
            status = normalize_tar_path(header, path, sizeof(path));
            if (status != SCDK_OK) {
                return status;
            }

            status = remember_file(path, base + data_offset, file_size);
            if (status != SCDK_OK) {
                return status;
            }
        }

        offset = data_offset + padded_size;
    }

    return initrd_file_count == 0u ? SCDK_ERR_NOENT : SCDK_OK;
}

static struct limine_file *find_initrd_module(void) {
    if (limine_modules == 0 || limine_modules->module_count == 0u) {
        return 0;
    }

    for (uint64_t i = 0; i < limine_modules->module_count; i++) {
        struct limine_file *file = limine_modules->modules[i];

        if (file == 0) {
            continue;
        }

        if (str_eq(file->string, "scdk.initrd") ||
            str_ends_with(file->path, "/scdk.initrd")) {
            return file;
        }
    }

    return limine_modules->modules[0];
}

scdk_status_t scdk_initrd_init_from_limine(void) {
    struct limine_file *module = find_initrd_module();
    scdk_status_t status;

    if (module == 0 || module->address == 0 || module->size == 0u) {
        return SCDK_ERR_NOENT;
    }

    status = parse_tar_image(module->address, module->size);
    if (status != SCDK_OK) {
        return status;
    }

    initrd_initialized = true;
    scdk_log_write("initrd", "found module");
    return SCDK_OK;
}

scdk_status_t scdk_initrd_list(void) {
    if (!initrd_initialized) {
        return SCDK_ERR_NOTSUP;
    }

    for (uint32_t i = 0; i < initrd_file_count; i++) {
        scdk_log_write("initrd", "file: %s", initrd_files[i].path);
    }

    scdk_log_write("initrd", "list pass");
    return SCDK_OK;
}

scdk_status_t scdk_initrd_find(const char *path,
                               struct scdk_initrd_file *out_file) {
    if (!initrd_initialized) {
        return SCDK_ERR_NOTSUP;
    }

    if (path == 0 || out_file == 0) {
        return SCDK_ERR_INVAL;
    }

    for (uint32_t i = 0; i < initrd_file_count; i++) {
        if (!str_eq(path, initrd_files[i].path)) {
            continue;
        }

        out_file->path = initrd_files[i].path;
        out_file->data = initrd_files[i].data;
        out_file->size = initrd_files[i].size;
        out_file->flags = initrd_files[i].flags;
        return SCDK_OK;
    }

    return SCDK_ERR_NOENT;
}

static size_t normalized_query_len(const char *path) {
    size_t len = strlen(path);

    while (len > 1u && path[len - 1u] == '/') {
        len--;
    }

    return len;
}

static bool path_eq_len(const char *path, const char *query, size_t query_len) {
    return strlen(path) == query_len && memcmp(path, query, query_len) == 0;
}

static bool path_has_dir_prefix(const char *path,
                                const char *query,
                                size_t query_len) {
    if (query_len == 1u && query[0] == '/') {
        return path[0] == '/' && path[1] != '\0';
    }

    return strlen(path) > query_len + 1u &&
           memcmp(path, query, query_len) == 0 &&
           path[query_len] == '/';
}

static bool dirent_name_eq(const char *lhs, const char *rhs, size_t rhs_len) {
    return strlen(lhs) == rhs_len && memcmp(lhs, rhs, rhs_len) == 0;
}

static scdk_status_t add_dirent(struct scdk_vfs_dirent *entries,
                                uint64_t capacity,
                                uint64_t *inout_count,
                                const char *name,
                                size_t name_len,
                                uint64_t type,
                                uint64_t size) {
    struct scdk_vfs_dirent *entry;

    if (entries == 0 || inout_count == 0 || name == 0 || name_len == 0u) {
        return SCDK_ERR_INVAL;
    }

    if (name_len >= SCDK_VFS_MAX_NAME) {
        return SCDK_ERR_BOUNDS;
    }

    for (uint64_t i = 0; i < *inout_count; i++) {
        if (!dirent_name_eq(entries[i].name, name, name_len)) {
            continue;
        }

        if (type == SCDK_VFS_NODE_DIR) {
            entries[i].type = SCDK_VFS_NODE_DIR;
            entries[i].size = 0;
        }
        return SCDK_OK;
    }

    if (*inout_count >= capacity) {
        return SCDK_ERR_BOUNDS;
    }

    entry = &entries[*inout_count];
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->name, name, name_len);
    entry->name[name_len] = '\0';
    entry->type = type;
    entry->size = size;
    *inout_count += 1u;
    return SCDK_OK;
}

scdk_status_t scdk_initrd_stat(const char *path,
                               struct scdk_vfs_stat *out_stat) {
    size_t query_len;

    if (!initrd_initialized) {
        return SCDK_ERR_NOTSUP;
    }

    if (path == 0 || out_stat == 0 || path[0] != '/') {
        return SCDK_ERR_INVAL;
    }

    memset(out_stat, 0, sizeof(*out_stat));
    query_len = normalized_query_len(path);

    if (query_len == 1u) {
        out_stat->type = SCDK_VFS_NODE_DIR;
        out_stat->size = 0;
        return SCDK_OK;
    }

    for (uint32_t i = 0; i < initrd_file_count; i++) {
        if (path_eq_len(initrd_files[i].path, path, query_len)) {
            out_stat->type = SCDK_VFS_NODE_FILE;
            out_stat->size = initrd_files[i].size;
            return SCDK_OK;
        }
    }

    for (uint32_t i = 0; i < initrd_file_count; i++) {
        if (path_has_dir_prefix(initrd_files[i].path, path, query_len)) {
            out_stat->type = SCDK_VFS_NODE_DIR;
            out_stat->size = 0;
            return SCDK_OK;
        }
    }

    return SCDK_ERR_NOENT;
}

scdk_status_t scdk_initrd_listdir(const char *path,
                                  struct scdk_vfs_dirent *entries,
                                  uint64_t capacity,
                                  uint64_t *out_count) {
    size_t query_len;
    uint64_t count = 0;
    struct scdk_vfs_stat stat;
    scdk_status_t status;

    if (!initrd_initialized) {
        return SCDK_ERR_NOTSUP;
    }

    if (path == 0 || entries == 0 || out_count == 0 || capacity == 0u ||
        path[0] != '/') {
        return SCDK_ERR_INVAL;
    }

    query_len = normalized_query_len(path);
    status = scdk_initrd_stat(path, &stat);
    if (status != SCDK_OK) {
        return status;
    }

    if (stat.type != SCDK_VFS_NODE_DIR) {
        return SCDK_ERR_NOTSUP;
    }

    memset(entries, 0, sizeof(entries[0]) * (size_t)capacity);

    for (uint32_t i = 0; i < initrd_file_count; i++) {
        const char *entry_path = initrd_files[i].path;
        const char *remainder;
        const char *slash;
        size_t name_len = 0;
        uint64_t type = SCDK_VFS_NODE_FILE;
        uint64_t size = initrd_files[i].size;

        if (!path_has_dir_prefix(entry_path, path, query_len)) {
            continue;
        }

        if (query_len == 1u) {
            remainder = entry_path + 1u;
        } else {
            remainder = entry_path + query_len + 1u;
        }

        if (remainder[0] == '\0') {
            continue;
        }

        slash = remainder;
        while (slash[name_len] != '\0' && slash[name_len] != '/') {
            name_len++;
        }

        if (slash[name_len] == '/') {
            type = SCDK_VFS_NODE_DIR;
            size = 0;
        }

        status = add_dirent(entries,
                            capacity,
                            &count,
                            remainder,
                            name_len,
                            type,
                            size);
        if (status != SCDK_OK) {
            return status;
        }
    }

    *out_count = count;
    return SCDK_OK;
}
