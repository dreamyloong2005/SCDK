// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_LOG_H
#define SCDK_LOG_H

#include <stdarg.h>

#include <scdk/capability.h>
#include <scdk/types.h>

/*
 * Control-plane: route normal logs through the console service endpoint.
 * Requires SCDK_RIGHT_SEND on an SCDK_OBJ_ENDPOINT capability.
 */
scdk_status_t scdk_log_set_console_endpoint(scdk_cap_t endpoint);

void scdk_log_write(const char *level, const char *fmt, ...);
void scdk_log_vwrite(const char *level, const char *fmt, va_list args);
void scdk_log_info(const char *fmt, ...);
void scdk_log_warn(const char *fmt, ...);
void scdk_log_error(const char *fmt, ...);

#endif
