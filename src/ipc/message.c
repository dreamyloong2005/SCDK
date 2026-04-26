// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#include <scdk/message.h>

#include <stddef.h>
#include <stdint.h>

void scdk_message_init(struct scdk_message *msg,
                       uint64_t sender,
                       uint64_t target,
                       uint64_t type) {
    if (msg == 0) {
        return;
    }

    msg->sender = sender;
    msg->target = target;
    msg->type = type;
    msg->arg0 = 0;
    msg->arg1 = 0;
    msg->arg2 = 0;
    msg->arg3 = 0;
    msg->status = (uint64_t)SCDK_ERR_INVAL;
}

scdk_status_t scdk_message_init_write(struct scdk_message *msg,
                                      uint64_t sender,
                                      uint64_t target,
                                      const char *buffer,
                                      uint64_t length) {
    if (msg == 0 || buffer == 0) {
        return SCDK_ERR_INVAL;
    }

    scdk_message_init(msg, sender, target, SCDK_MSG_WRITE);
    msg->arg0 = (uint64_t)(uintptr_t)buffer;
    msg->arg1 = length;
    msg->status = (uint64_t)SCDK_OK;
    return SCDK_OK;
}
