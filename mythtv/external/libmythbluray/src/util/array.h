/*
 * This file is part of libbluray
 * Copyright (C) 2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#ifndef BD_ARRAY_H_
#define BD_ARRAY_H_

#include "attributes.h"

#include <stddef.h>

/*
 * array_alloc()
 *
 * Allocate an array of objects.
 * Each object is initialized with zeros.
 *
 * @param  n   number of objects
 * @param  sz  size of single object
 * @return     array of n pointers, each pointing to memory block of size sz.
 *
 */
BD_PRIVATE void *array_alloc(size_t n, size_t sz) BD_ATTR_MALLOC;

/*
 * array_free()
 *
 * Free array allocated with array_alloc().
 *
 * @param p  pointer to pointer allocated with bd_array_alloc()
 */
BD_PRIVATE void array_free(void **p);

#endif // BD_ARRAY_H_
