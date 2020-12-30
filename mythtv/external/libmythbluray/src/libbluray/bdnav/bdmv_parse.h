/*
 * This file is part of libbluray
 * Copyright (C) 2017  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_BDMV_PARSE_H_)
#define _BDMV_PARSE_H_

#include "util/attributes.h"
#include "util/bits.h"

#include <stdint.h>

#define BDMV_VERSION_0100 ('0' << 24 | '1' << 16 | '0' << 8 | '0')
#define BDMV_VERSION_0200 ('0' << 24 | '2' << 16 | '0' << 8 | '0')
#define BDMV_VERSION_0240 ('0' << 24 | '2' << 16 | '4' << 8 | '0')
#define BDMV_VERSION_0300 ('0' << 24 | '3' << 16 | '0' << 8 | '0')

BD_PRIVATE int bdmv_parse_header(BITSTREAM *bs, uint32_t type, uint32_t *version);

#endif // _BDMV_PARSE_H_
