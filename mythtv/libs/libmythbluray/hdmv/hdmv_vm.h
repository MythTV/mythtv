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

#if !defined(_HDMV_VM_H_)
#define _HDMV_VM_H_

#include <util/attributes.h>

#include <stdint.h>

/*
 * events from hdmv_run()
 */

typedef enum {
    HDMV_EVENT_NONE = 0,       /* no events */
    HDMV_EVENT_END,            /* end of program */

    HDMV_EVENT_TITLE,          /* play title (from disc index) */
    HDMV_EVENT_PLAY_PL,        /* select playlist */
    HDMV_EVENT_PLAY_PI,        /* seek to playitem */
    HDMV_EVENT_PLAY_PM,        /* seek to playmark */
    HDMV_EVENT_PLAY_STOP,      /* stop playing playlist */

    HDMV_EVENT_STILL,          /* param: boolean */

    HDMV_EVENT_ENABLE_BUTTON,
    HDMV_EVENT_DISABLE_BUTTON,
    HDMV_EVENT_POPUP_OFF,

} hdmv_event_e;

typedef struct hdmv_vm_event_s {
    hdmv_event_e event;
    uint32_t     param;
} HDMV_EVENT;

/*
 *
 */

struct bd_registers_s;

/*
 *
 */

typedef struct hdmv_vm_s HDMV_VM;

BD_PRIVATE HDMV_VM *hdmv_vm_init(const char *disc_root, struct bd_registers_s *regs);
BD_PRIVATE void     hdmv_vm_free(HDMV_VM *p);

BD_PRIVATE int      hdmv_vm_select_object(HDMV_VM *p, int object, void *ig_object);
BD_PRIVATE int      hdmv_vm_run(HDMV_VM *p, HDMV_EVENT *ev);
BD_PRIVATE int      hdmv_vm_get_event(HDMV_VM *p, HDMV_EVENT *ev);

#endif // _HDMV_VM_H_
