/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef LOGGING_H_
#define LOGGING_H_

#include "log_control.h"

#include "attributes.h"

#include <stdint.h>

#define BD_DEBUG(MASK,...) bd_debug(__FILE__,__LINE__,MASK,__VA_ARGS__)


BD_PRIVATE char *print_hex(char *out, const uint8_t *str, int count);
BD_PRIVATE void bd_debug(const char *file, int line, uint32_t mask, const char *format, ...) BD_ATTR_FORMAT_PRINTF(4,5);


#endif /* LOGGING_H_ */
