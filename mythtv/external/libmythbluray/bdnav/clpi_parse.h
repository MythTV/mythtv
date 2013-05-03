/*
 * This file is part of libbluray
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

#if !defined(_CLPI_PARSE_H_)
#define _CLPI_PARSE_H_

#include "clpi_data.h"
#include "util/attributes.h"

#include <stdint.h>

BD_PRIVATE uint32_t clpi_lookup_spn(const CLPI_CL *cl, uint32_t timestamp, int before, uint8_t stc_id);
BD_PRIVATE uint32_t clpi_access_point(const CLPI_CL *cl, uint32_t pkt, int next, int angle_change, uint32_t *time);
BD_PRIVATE CLPI_CL* clpi_parse(const char *path, int verbose);
BD_PRIVATE CLPI_CL* clpi_copy(const CLPI_CL* src_cl);
BD_PRIVATE void clpi_free(CLPI_CL *cl);

#endif // _CLPI_PARSE_H_
