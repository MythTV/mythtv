/*
 * This file is part of libbluray
 * Copyright (C) 2010  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_MOBJ_PARSE_H_)
#define _MOBJ_PARSE_H_

#include "util/attributes.h"

#include <stdint.h>

struct bd_disc;
struct mobj_objects;
struct mobj_cmd;

BD_PRIVATE struct mobj_objects* mobj_parse(const char *file) BD_ATTR_MALLOC; /* parse MovieObject.bdmv */
BD_PRIVATE struct mobj_objects* mobj_get(struct bd_disc *disc);              /* parse MovieObject.bdmv */
BD_PRIVATE void                 mobj_parse_cmd(uint8_t *buf, struct mobj_cmd *cmd);
BD_PRIVATE void                 mobj_free(struct mobj_objects **index);

#endif // _MOBJ_PARSE_H_
