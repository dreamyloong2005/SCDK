// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_TTY_H
#define SCDK_TTY_H

#include <scdk/capability.h>
#include <scdk/input.h>
#include <scdk/types.h>

struct scdk_tty_info {
    uint32_t event_size;
    uint32_t flags;
};

#define SCDK_TTY_INFO_POLLING 1u

/*
 * Control-plane: start the kernel-resident TTY/input service and register its
 * endpoint as SCDK_SERVICE_TTY.
 */
scdk_status_t scdk_tty_service_init(scdk_cap_t *out_endpoint);

/*
 * ABI helper: pack an input event into an endpoint message's return args.
 */
void scdk_tty_pack_event(const struct scdk_input_event *event,
                         uint64_t *arg0,
                         uint64_t *arg1,
                         uint64_t *arg2,
                         uint64_t *arg3);

#endif
