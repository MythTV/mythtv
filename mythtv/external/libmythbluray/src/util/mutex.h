/*
 * This file is part of libbluray
 * Copyright (C) 2010-2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#ifndef LIBBLURAY_MUTEX_H_
#define LIBBLURAY_MUTEX_H_

#include "attributes.h"

/*
 * recursive mutex
 */

typedef struct bd_mutex_s BD_MUTEX;
struct bd_mutex_s {
    void *impl;
};

BD_PRIVATE int bd_mutex_init(BD_MUTEX *p);
BD_PRIVATE int bd_mutex_destroy(BD_MUTEX *p);

BD_PRIVATE int bd_mutex_lock(BD_MUTEX *p);
BD_PRIVATE int bd_mutex_unlock(BD_MUTEX *p);

#endif // LIBBLURAY_MUTEX_H_
