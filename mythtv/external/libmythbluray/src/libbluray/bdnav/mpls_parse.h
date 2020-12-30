/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  John Stebbins
 * Copyright (C) 2012-2016  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_MPLS_PARSE_H_)
#define _MPLS_PARSE_H_

#include "util/attributes.h"

#include <stdint.h>

struct bd_disc;
struct mpls_pl;

BD_PRIVATE struct mpls_pl *mpls_parse(const char *path);
BD_PRIVATE struct mpls_pl *mpls_get(struct bd_disc *disc, const char *file);
BD_PRIVATE void mpls_free(struct mpls_pl **pl);

#endif // _MPLS_PARSE_H_
