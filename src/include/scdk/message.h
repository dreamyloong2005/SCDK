// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_MESSAGE_H
#define SCDK_MESSAGE_H

#include <stdint.h>

#include <scdk/types.h>

enum scdk_msg_type {
    SCDK_MSG_NONE = 0,

    SCDK_MSG_OPEN,
    SCDK_MSG_CLOSE,
    SCDK_MSG_READ,
    SCDK_MSG_WRITE,

    SCDK_MSG_DEVICE_REGISTER,
    SCDK_MSG_DEVICE_QUEUE_BIND,

    SCDK_MSG_SERVICE_REGISTER,
    SCDK_MSG_SERVICE_LOOKUP,

    SCDK_MSG_PROCESS_SPAWN,
    SCDK_MSG_PROCESS_EXIT,
    SCDK_MSG_PROCESS_WAIT_STUB,

    SCDK_MSG_RING_PROCESS,

    SCDK_MSG_CONSOLE_WRITE = 0x2000,
    SCDK_MSG_CONSOLE_CLEAR,
    SCDK_MSG_CONSOLE_GET_INFO,
    SCDK_MSG_CONSOLE_BIND_OUTPUT_RING,

    SCDK_MSG_TTY_POLL_EVENT = 0x2100,
    SCDK_MSG_TTY_BIND_INPUT_RING,
    SCDK_MSG_TTY_GET_INFO,

    SCDK_MSG_STAT = 0x3000,
    SCDK_MSG_LISTDIR,
};

struct scdk_message {
    uint64_t sender;
    uint64_t target;
    uint64_t type;

    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;

    uint64_t status;
};

/*
 * Control-plane: initialize a message envelope for endpoint delivery.
 * Requires no capability; endpoint delivery performs the capability check.
 */
void scdk_message_init(struct scdk_message *msg,
                       uint64_t sender,
                       uint64_t target,
                       uint64_t type);

/*
 * Control-plane prototype helper: build an SCDK_MSG_WRITE message.
 * The buffer is a bootstrap kernel pointer until grants exist.
 */
scdk_status_t scdk_message_init_write(struct scdk_message *msg,
                                      uint64_t sender,
                                      uint64_t target,
                                      const char *buffer,
                                      uint64_t length);

#endif
