// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_ENDPOINT_H
#define SCDK_ENDPOINT_H

#include <stdint.h>

#include <scdk/capability.h>
#include <scdk/message.h>
#include <scdk/types.h>

#define SCDK_MAX_ENDPOINTS 64u

typedef scdk_status_t (*scdk_endpoint_handler_t)(scdk_cap_t endpoint,
                                                 struct scdk_message *msg,
                                                 void *context);

/*
 * Control-plane: create an endpoint object and mint its bootstrap capability.
 * The returned cap carries SEND for callers and BIND for service registration.
 */
scdk_status_t scdk_endpoint_create(uint32_t owner_core,
                                   scdk_endpoint_handler_t handler,
                                   void *context,
                                   scdk_cap_t *out_endpoint);

/*
 * Control-plane prototype: deliver a message to an endpoint.
 * Requires SCDK_RIGHT_SEND on an SCDK_OBJ_ENDPOINT capability.
 */
scdk_status_t scdk_endpoint_call(scdk_cap_t endpoint,
                                 struct scdk_message *msg);

#endif
