// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 The Scadek OS Project contributors

#ifndef SCDK_SERIAL_H
#define SCDK_SERIAL_H

#include <stdbool.h>

bool scdk_serial_init(void);
void scdk_serial_write_char(char c);
void scdk_serial_write_string(const char *s);

#endif
