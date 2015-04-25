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

#if !defined(_MOBJ_PRINT_H_)
#define _MOBJ_PRINT_H_

#include "util/attributes.h"

struct mobj_cmd;

BD_PRIVATE int mobj_sprint_cmd(char *buf, struct mobj_cmd *cmd); /* print MOBJ_CMD to string. buf is expected to be 256 bytes. */

#endif // _MOBJ_PRINT_H_
