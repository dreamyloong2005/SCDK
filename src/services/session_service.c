// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/session.h>

#include <stdbool.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/service.h>

static bool session_started;

static bool session_service_allowed(uint64_t service_id) {
    switch (service_id) {
    case SCDK_SERVICE_CONSOLE:
    case SCDK_SERVICE_VFS:
    case SCDK_SERVICE_PROC:
    case SCDK_SERVICE_TTY:
    case SCDK_SERVICE_GRANT_TEST:
        return true;
    default:
        return false;
    }
}

static scdk_status_t session_endpoint_handler(scdk_cap_t endpoint,
                                              struct scdk_message *msg,
                                              void *context) {
    scdk_cap_t service_endpoint = 0;
    scdk_status_t status;

    (void)endpoint;
    (void)context;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    switch (msg->type) {
    case SCDK_MSG_SERVICE_LOOKUP:
        if (!session_service_allowed(msg->arg0)) {
            return SCDK_ERR_PERM;
        }

        status = scdk_service_lookup(msg->arg0, &service_endpoint);
        if (status != SCDK_OK) {
            return status;
        }

        msg->arg0 = service_endpoint;
        msg->arg1 = 0;
        msg->arg2 = 0;
        msg->arg3 = 0;
        return SCDK_OK;
    default:
        return SCDK_ERR_NOTSUP;
    }
}

scdk_status_t scdk_session_service_init(scdk_cap_t *out_endpoint) {
    scdk_cap_t endpoint = 0;
    scdk_status_t status;

    if (out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    if (session_started) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  session_endpoint_handler,
                                  0,
                                  &endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_service_register(SCDK_SERVICE_SESSION, endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    session_started = true;
    *out_endpoint = endpoint;
    scdk_log_write("m31", "session service endpoint registered");
    return SCDK_OK;
}
