/*
 * This file is part of libbluray
 * Copyright (C) 2010-2016  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(BD_EVENT_QUEUE_H_)
#define BD_EVENT_QUEUE_H_

#include "util/attributes.h"

#include <stddef.h>

typedef struct bd_event_queue BD_EVENT_QUEUE;

BD_PRIVATE BD_EVENT_QUEUE *event_queue_new(size_t event_size);
BD_PRIVATE void            event_queue_destroy(BD_EVENT_QUEUE **);

BD_PRIVATE int event_queue_get(BD_EVENT_QUEUE *eq, void *ev);
BD_PRIVATE int event_queue_put(BD_EVENT_QUEUE *eq, const void *ev);

#endif /* BD_EVENT_QUEUE_H_ */
