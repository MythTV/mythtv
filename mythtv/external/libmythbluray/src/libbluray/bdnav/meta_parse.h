/*
 * This file is part of libbluray
 * Copyright (C) 2010 fraxinas
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

#if !defined(_META_PARSE_H_)
#define _META_PARSE_H_

#include "util/attributes.h"

struct bd_disc;
struct meta_root;

BD_PRIVATE struct meta_root *     meta_parse(struct bd_disc *disc) BD_ATTR_MALLOC;
BD_PRIVATE void                   meta_free (struct meta_root **index);
BD_PRIVATE const struct meta_dl * meta_get  (const struct meta_root *meta_root, const char *language_code);

#endif // _META_PARSE_H_

