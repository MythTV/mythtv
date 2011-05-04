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
    HDMV_EVENT_END,            /* end of program (movie object) */
    HDMV_EVENT_IG_END,         /* end of program (interactive) */

    HDMV_EVENT_TITLE,          /* play title (from disc index) */
    HDMV_EVENT_PLAY_PL,        /* select playlist */
    HDMV_EVENT_PLAY_PI,        /* seek to playitem */
    HDMV_EVENT_PLAY_PM,        /* seek to playmark */
    HDMV_EVENT_PLAY_STOP,      /* stop playing playlist */

    HDMV_EVENT_STILL,          /* param: boolean */

    HDMV_EVENT_SET_BUTTON_PAGE,
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
struct indx_root_s;

/*
 *
 */

typedef struct hdmv_vm_s HDMV_VM;

BD_PRIVATE HDMV_VM *hdmv_vm_init(const char *disc_root, struct bd_registers_s *regs, struct indx_root_s *indx);
BD_PRIVATE void     hdmv_vm_free(HDMV_VM **p);

BD_PRIVATE int      hdmv_vm_select_object(HDMV_VM *p, int object);
BD_PRIVATE int      hdmv_vm_set_object(HDMV_VM *p, int num_nav_cmds, void *nav_cmds);
BD_PRIVATE int      hdmv_vm_run(HDMV_VM *p, HDMV_EVENT *ev);
BD_PRIVATE int      hdmv_vm_get_event(HDMV_VM *p, HDMV_EVENT *ev);

BD_PRIVATE int      hdmv_vm_running(HDMV_VM *p);

#define HDMV_MENU_CALL_MASK     0x01
#define HDMV_TITLE_SEARCH_MASK  0x02
BD_PRIVATE uint32_t hdmv_vm_get_uo_mask(HDMV_VM *p);

/**
 *
 *  Suspend playlist playback
 *
 *  This function assumes playlist is currently playing and
 *  movie object execution is suspended at PLAY_PL instruction.
 *
 *  If resume_intention_flag of current movie object is 1:
 *    Copy playback position PSRs to backup registers
 *    (suspend playlist playback at current position)
 *  If resume_intention_flag of current movie object is 0:
 *    Discard current movie object
 *
 * @param p  HDMV_VM object
 * @return 0 on success, -1 if error
 */
BD_PRIVATE int      hdmv_vm_suspend_pl(HDMV_VM *p);

/**
 *
 *  Resume HDMV execution
 *
 *  Continue execution of movie object after playlist playback.
 *  Do not restore backup PSRs.
 *  This function is called when playlist playback ends.
 *
 * @param p  HDMV_VM object
 * @return 0 on success, -1 if error
 */
BD_PRIVATE int      hdmv_vm_resume(HDMV_VM *p);

#endif // _HDMV_VM_H_
