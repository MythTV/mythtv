/*
 * This file is part of libbluray
 * Copyright (C) 2010  hpi1
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

#include "pes_buffer.h"

#include "util/macro.h"

#include <stdlib.h>
#include <string.h>

PES_BUFFER *pes_buffer_alloc(int size)
{
    PES_BUFFER *p = calloc(1, sizeof(*p));

    if (p) {
        p->size = size;
        p->buf = malloc(size);
    }

    return p;
}

void pes_buffer_free(PES_BUFFER **p)
{
    if (p && *p) {
        if ((*p)->next) {
            pes_buffer_free(&(*p)->next);
        }
        X_FREE ((*p)->buf);
        X_FREE (*p);
    }
}

void pes_buffer_append(PES_BUFFER **head, PES_BUFFER *buf)
{
    if (!head) {
        return;
    }

    if (!*head) {
        *head = buf;
        return;
    }

    if (buf) {
        PES_BUFFER *tail = *head;
        for (; tail->next; tail = tail->next) ;
        tail->next = buf;
    }
}

static PES_BUFFER *_prev_buffer(PES_BUFFER *head, PES_BUFFER *buf)
{
    while (head) {
        if (head->next == buf) {
            return head;
        }
        head = head->next;
    }

    return NULL;
}

void pes_buffer_remove(PES_BUFFER **head, PES_BUFFER *p)
{
    if (head && *head && p) {
        if (*head == p) {
            *head = (*head)->next;
            p->next = NULL;
            pes_buffer_free(&p);
        } else {
            PES_BUFFER *prev = _prev_buffer(*head, p);
            if (prev) {
                prev->next = p->next;
                p->next = NULL;
                pes_buffer_free(&p);
            }
        }
    }
}
