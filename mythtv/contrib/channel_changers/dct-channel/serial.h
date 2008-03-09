/*
 * dct-channel
 * Copyright (c) 2003 Jim Paris <jim@jtan.com>
 *
 * This is free software; you can redistribute it and/or modify it and
 * it is provided under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation; see COPYING.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include "packet.h"

/* Only one port open at a time. */
int serial_init(char *device);
void serial_close();
void serial_flush();

int serial_getpacket(packet *p, int timeout_msec);
void serial_sendpacket(packet *p);

#endif
