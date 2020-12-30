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

#include "util/attributes.h"

#include <stdint.h>

struct clpi_cl;
struct bd_disc;

BD_PRIVATE struct clpi_cl* clpi_parse(const char *path);
BD_PRIVATE struct clpi_cl* clpi_get(struct bd_disc *disc, const char *file);
BD_PRIVATE struct clpi_cl* clpi_copy(const struct clpi_cl* src_cl);
BD_PRIVATE void clpi_free(struct clpi_cl **cl);

BD_PRIVATE uint32_t clpi_find_stc_spn(const struct clpi_cl *cl, uint8_t stc_id);
BD_PRIVATE uint32_t clpi_lookup_spn(const struct clpi_cl *cl, uint32_t timestamp, int before, uint8_t stc_id);
BD_PRIVATE uint32_t clpi_access_point(const struct clpi_cl *cl, uint32_t pkt, int next, int angle_change, uint32_t *time);


#endif // _CLPI_PARSE_H_
