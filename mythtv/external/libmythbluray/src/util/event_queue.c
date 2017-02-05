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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "event_queue.h"

#include "util/macro.h"
#include "util/mutex.h"

#include <string.h>


#define MAX_EVENTS 31  /* 2^n - 1 */

struct bd_event_queue {
    BD_MUTEX mutex;
    size_t   event_size;
    unsigned in;  /* next free slot */
    unsigned out; /* next event */

    unsigned char ev[1];
};


void event_queue_destroy(BD_EVENT_QUEUE **pp)
{
    if (pp && *pp) {
        BD_EVENT_QUEUE *eq = *pp;
        bd_mutex_destroy(&eq->mutex);
        X_FREE(*pp);
    }
}

BD_EVENT_QUEUE *event_queue_new(size_t event_size)
{
    BD_EVENT_QUEUE *eq = calloc(1, sizeof(BD_EVENT_QUEUE) + event_size * (MAX_EVENTS + 1));
    if (eq) {
        bd_mutex_init(&eq->mutex);
        eq->event_size = event_size;
    }
    return eq;
}

int event_queue_get(BD_EVENT_QUEUE *eq, void *ev)
{
    int result = 0;

    if (eq) {
        bd_mutex_lock(&eq->mutex);

        if (eq->in != eq->out) {

            memcpy(ev, &eq->ev[eq->out * eq->event_size], eq->event_size);
            eq->out = (eq->out + 1) & MAX_EVENTS;

            result = 1;
        }

        bd_mutex_unlock(&eq->mutex);
    }

    return result;
}

int event_queue_put(BD_EVENT_QUEUE *eq, const void *ev)
{
    int result = 0;

    if (eq) {
        bd_mutex_lock(&eq->mutex);

        unsigned new_in = (eq->in + 1) & MAX_EVENTS;

        if (new_in != eq->out) {
            memcpy(&eq->ev[eq->in * eq->event_size], ev, eq->event_size);
            eq->in = new_in;

            result = 1;
        }

        bd_mutex_unlock(&eq->mutex);
    }

    return result;
}
