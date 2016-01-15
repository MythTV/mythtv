/*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#ifndef BD_REFCNT_H_
#define BD_REFCNT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "attributes.h"

#include <stddef.h>

#ifdef MACRO_H_
#  error macro.h included before refcnt.h
#endif

/*
 * Reference-counted memory blocks.
 *
 * - Object must be allocated with refcnt_realloc(NULL, size).
 *   Returned object has reference count of 1.
 * - Object can be re-allocated with refcnt_realloc(obj, size)
 *   as long as bd_refcnt_inc() has not been called.
 * - Object must be freed with bd_refcnt_dec().
 *
 * by default, reference counting is not used (use count = 1).
 * Reference counting is initialized during first call to bd_refcnt_inc().
 * This results in reference count = 2.
 *
 * This is thread-safe as long as first bd_refcnt_inc() is done from the
 * same thread that owns the object initially.
 *
 */

BD_PRIVATE void *refcnt_realloc(void *obj, size_t sz);

#ifndef BD_OVERLAY_INTERFACE_VERSION
void bd_refcnt_inc(const void *obj);
void bd_refcnt_dec(const void *obj);
#endif

#ifdef __cplusplus
}
#endif

#endif // BD_REFCNT_H_
