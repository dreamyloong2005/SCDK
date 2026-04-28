// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/tty.h>

#include <stdbool.h>
#include <stdint.h>

#include <scdk/endpoint.h>
#include <scdk/keyboard.h>
#include <scdk/log.h>
#include <scdk/message.h>
#include <scdk/service.h>

static bool tty_started;

void scdk_tty_pack_event(const struct scdk_input_event *event,
                         uint64_t *arg0,
                         uint64_t *arg1,
                         uint64_t *arg2,
                         uint64_t *arg3) {
    if (event == 0 || arg0 == 0 || arg1 == 0 || arg2 == 0 || arg3 == 0) {
        return;
    }

    *arg0 = event->timestamp;
    *arg1 = ((uint64_t)event->type << 32u) | event->keycode;
    *arg2 = event->ascii;
    *arg3 = ((uint64_t)event->modifiers << 32u) | event->flags;
}

static scdk_status_t tty_endpoint_handler(scdk_cap_t endpoint,
                                          struct scdk_message *msg,
                                          void *context) {
    (void)endpoint;
    (void)context;

    if (msg == 0) {
        return SCDK_ERR_INVAL;
    }

    switch (msg->type) {
    case SCDK_MSG_TTY_POLL_EVENT: {
        struct scdk_input_event event;
        scdk_status_t status = scdk_keyboard_poll(&event);
        if (status != SCDK_OK) {
            return status;
        }

        scdk_tty_pack_event(&event, &msg->arg0, &msg->arg1, &msg->arg2, &msg->arg3);
        return SCDK_OK;
    }
    case SCDK_MSG_TTY_GET_INFO:
        msg->arg0 = sizeof(struct scdk_input_event);
        msg->arg1 = SCDK_TTY_INFO_POLLING;
        msg->arg2 = 0;
        msg->arg3 = 0;
        return SCDK_OK;
    case SCDK_MSG_TTY_BIND_INPUT_RING:
        return SCDK_ERR_NOTSUP;
    default:
        return SCDK_ERR_NOTSUP;
    }
}

scdk_status_t scdk_tty_service_init(scdk_cap_t *out_endpoint) {
    scdk_cap_t endpoint = 0;
    scdk_status_t status;

    if (out_endpoint == 0) {
        return SCDK_ERR_INVAL;
    }

    if (tty_started) {
        return SCDK_ERR_BUSY;
    }

    status = scdk_keyboard_init();
    if (status != SCDK_OK) {
        return status;
    }
    scdk_log_write("input", "ps2 keyboard backend ok");
    scdk_log_write("input", "ps2 keyboard init ok");

    status = scdk_endpoint_create(SCDK_BOOT_CORE,
                                  tty_endpoint_handler,
                                  0,
                                  &endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    status = scdk_service_register(SCDK_SERVICE_TTY, endpoint);
    if (status != SCDK_OK) {
        return status;
    }

    tty_started = true;
    *out_endpoint = endpoint;
    scdk_log_write("tty", "service endpoint registered");
    return SCDK_OK;
}
