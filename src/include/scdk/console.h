// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_CONSOLE_H
#define SCDK_CONSOLE_H

#include <scdk/capability.h>
#include <scdk/console_backend.h>
#include <scdk/types.h>

/*
 * SCDK-native console service ABI. SCDK_MSG_WRITE remains as a bootstrap
 * kernel-buffer compatibility path; normal userspace writes should use
 * SCDK_MSG_CONSOLE_WRITE with a grant-backed buffer.
 */
#define SCDK_CONSOLE_WRITE_SERIAL_MIRROR 1u

scdk_status_t scdk_console_service_init(scdk_cap_t *out_endpoint);

#endif
