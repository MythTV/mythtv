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

#include "hdmv_vm.h"

#include "mobj_parse.h"
#include "hdmv_insn.h"
#include "../register.h"

#include "../bdnav/index_parse.h"
#include "util/macro.h"
#include "util/strutl.h"
#include "util/logging.h"
#include "util/mutex.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>


typedef struct {
    time_t   time;
    uint32_t mobj_id;
} NV_TIMER;

struct hdmv_vm_s {

    BD_MUTEX       mutex;

    /* state */
    uint32_t       pc;            /* program counter */
    BD_REGISTERS  *regs;          /* player registers */
    MOBJ_OBJECT   *object;        /* currently running object code */

    HDMV_EVENT     event[5];      /* pending events to return */

    NV_TIMER       nv_timer;      /* navigation timer */

    /* movie objects */
    MOBJ_OBJECTS  *movie_objects; /* disc movie objects */
    MOBJ_OBJECT   *ig_object;     /* current object from IG stream */

    /* object currently playing playlist */
    MOBJ_OBJECT *playing_object;
    int          playing_pc;

    /* suspended object */
    MOBJ_OBJECT *suspended_object;
    int          suspended_pc;

    /* disc index (used to verify CALL_TITLE/JUMP_TITLE) */
    INDX_ROOT   *indx;
};

/*
 * registers: PSR and GPR access
 */

#define PSR_FLAG 0x80000000

static int _is_valid_reg(uint32_t reg)
{
    if (reg & PSR_FLAG) {
        if (reg & ~0x8000007f) {
            return 0;
        }
    }  else {
        if (reg & ~0x00000fff) {
            return 0;
        }
    }
    return 1;
}

static int _store_reg(HDMV_VM *p, uint32_t reg, uint32_t val)
{
    if (!_is_valid_reg(reg)) {
        BD_DEBUG(DBG_HDMV, "_store_reg(): invalid register 0x%x\n", reg);
        return -1;
    }

    if (reg & PSR_FLAG) {
        BD_DEBUG(DBG_HDMV, "_store_reg(): storing to PSR is not allowed\n");
        return -1;
    }  else {
        return bd_gpr_write(p->regs, reg, val);
    }
}

static uint32_t _read_reg(HDMV_VM *p, uint32_t reg)
{
    if (!_is_valid_reg(reg)) {
        BD_DEBUG(DBG_HDMV, "_read_reg(): invalid register 0x%x\n", reg);
        return 0;
    }

    if (reg & PSR_FLAG) {
        return bd_psr_read(p->regs, reg & 0x7f);
    } else {
        return bd_gpr_read(p->regs, reg);
    }
}

static uint32_t _read_setstream_regs(HDMV_VM *p, uint32_t val)
{
    uint32_t flags = val & 0xf000f000;
    uint32_t reg0 = val & 0xfff;
    uint32_t reg1 = (val >> 16) & 0xfff;

    uint32_t val0 = bd_gpr_read(p->regs, reg0) & 0x0fff;
    uint32_t val1 = bd_gpr_read(p->regs, reg1) & 0x0fff;

    return flags | val0 | (val1 << 16);
}

static uint32_t _read_setbuttonpage_reg(HDMV_VM *p, uint32_t val)
{
    uint32_t flags = val & 0xc0000000;
    uint32_t reg0  = val & 0x00000fff;

    uint32_t val0  = bd_gpr_read(p->regs, reg0) & 0x3fffffff;

    return flags | val0;
}

static int _store_result(HDMV_VM *p, MOBJ_CMD *cmd, uint32_t src, uint32_t dst, uint32_t src0, uint32_t dst0)
{
    int ret = 0;

    /* store result to destination register(s) */
    if (dst != dst0) {
        if (cmd->insn.imm_op1) {
            BD_DEBUG(DBG_HDMV|DBG_CRIT, "ERROR: storing to imm ! ");
            return -1;
        }
        ret = _store_reg(p, cmd->dst, dst);
    }

    if (src != src0) {
        if (cmd->insn.imm_op1) {
            BD_DEBUG(DBG_HDMV|DBG_CRIT, "ERROR: storing to imm ! ");
            return -1;
        }
        ret += _store_reg(p, cmd->src, src);
    }

    return ret;
}

static uint32_t _fetch_operand(HDMV_VM *p, int setstream, int setbuttonpage, int imm, uint32_t value)
{
    if (imm) {
        return value;
    }

    if (setstream) {
        return _read_setstream_regs(p, value);

    } else if (setbuttonpage) {
        return _read_setbuttonpage_reg(p, value);

    } else {
        return _read_reg(p, value);
    }
}

static void _fetch_operands(HDMV_VM *p, MOBJ_CMD *cmd, uint32_t *dst, uint32_t *src)
{
    HDMV_INSN *insn = &cmd->insn;

    int setstream = (insn->grp     == INSN_GROUP_SET &&
                     insn->sub_grp == SET_SETSYSTEM  &&
                     (  insn->set_opt == INSN_SET_STREAM ||
                        insn->set_opt == INSN_SET_SEC_STREAM));
    int setbuttonpage = (insn->grp     == INSN_GROUP_SET &&
                         insn->sub_grp == SET_SETSYSTEM  &&
                         insn->set_opt == INSN_SET_BUTTON_PAGE);

    *dst = *src = 0;

    if (insn->op_cnt > 0) {
        *dst = _fetch_operand(p, setstream, setbuttonpage, insn->imm_op1, cmd->dst);
    }

    if (insn->op_cnt > 1) {
        *src = _fetch_operand(p, setstream, setbuttonpage, insn->imm_op2, cmd->src);
    }
}

/*
 * event queue
 */

static int _get_event(HDMV_VM *p, HDMV_EVENT *ev)
{
    if (p->event[0].event != HDMV_EVENT_NONE) {
        *ev = p->event[0];
        memmove(p->event, p->event + 1, sizeof(p->event) - sizeof(p->event[0]));
        return 0;
    }

    ev->event = HDMV_EVENT_NONE;

    return -1;
}

static int _queue_event(HDMV_VM *p, uint32_t event, uint32_t param)
{
    unsigned i;
    for (i = 0; i < sizeof(p->event) / sizeof(p->event[0]) - 1; i++) {
        if (p->event[i].event == HDMV_EVENT_NONE) {
            p->event[i].event = event;
            p->event[i].param = param;
            return 0;
        }
    }

    BD_DEBUG(DBG_HDMV|DBG_CRIT, "_queue_event(%d, %d): queue overflow !\n", event, param);
    return -1;
}

/*
 * vm init
 */

HDMV_VM *hdmv_vm_init(const char *disc_root, BD_REGISTERS *regs, INDX_ROOT *indx)
{
    HDMV_VM *p = calloc(1, sizeof(HDMV_VM));
    char *file;

    /* read movie objects */
    file = str_printf("%s/BDMV/MovieObject.bdmv", disc_root);
    p->movie_objects = mobj_parse(file);
    X_FREE(file);
    if (!p->movie_objects) {
        X_FREE(p);
        return NULL;
    }

    p->regs         = regs;
    p->indx         = indx;

    bd_mutex_init(&p->mutex);

    return  p;
}

static void _free_ig_object(HDMV_VM *p)
{
    if (p->ig_object) {
        X_FREE(p->ig_object->cmds);
        X_FREE(p->ig_object);
    }
}

void hdmv_vm_free(HDMV_VM **p)
{
    if (p && *p) {

        bd_mutex_destroy(&(*p)->mutex);

        mobj_free(&(*p)->movie_objects);

        _free_ig_object(*p);

        X_FREE(*p);
    }
}

/*
 * suspend/resume ("function call")
 */

static int _suspended_at_play_pl(HDMV_VM *p)
{
    int play_pl = 0;
    if (p && p->suspended_object) {
        MOBJ_CMD  *cmd  = &p->suspended_object->cmds[p->suspended_pc];
        HDMV_INSN *insn = &cmd->insn;
        play_pl = (insn->grp     == INSN_GROUP_BRANCH &&
                   insn->sub_grp == BRANCH_PLAY  &&
                   (  insn->branch_opt == INSN_PLAY_PL ||
                      insn->branch_opt == INSN_PLAY_PL_PI ||
                      insn->branch_opt == INSN_PLAY_PL_PM));
    }

    return play_pl;
}

static int _suspend_for_play_pl(HDMV_VM *p)
{
    if (p->playing_object) {
        BD_DEBUG(DBG_HDMV|DBG_CRIT, "_suspend_for_play_pl(): object already playing playlist !\n");
        return -1;
    }

    p->playing_object = p->object;
    p->playing_pc     = p->pc;

    p->object = NULL;

    return 0;
}

static int _resume_from_play_pl(HDMV_VM *p)
{
    if (!p->playing_object) {
        BD_DEBUG(DBG_HDMV|DBG_CRIT, "_resume_from_play_pl(): object not playing playlist !\n");
        return -1;
    }

    p->object = p->playing_object;
    p->pc     = p->playing_pc + 1;

    p->playing_object = NULL;

    _free_ig_object(p);

    return 0;
}

static void _suspend_object(HDMV_VM *p, int psr_backup)
{
    BD_DEBUG(DBG_HDMV, "_suspend_object()\n");

    if (p->suspended_object) {
        BD_DEBUG(DBG_HDMV, "_suspend_object: object already suspended !\n");
        // [execute the call, discard old suspended object (10.2.4.2.2)].
    }

    if (psr_backup) {
        bd_psr_save_state(p->regs);
    }

    if (p->ig_object) {
        if (!p->playing_object) {
            BD_DEBUG(DBG_HDMV|DBG_CRIT, "_suspend_object: IG object tries to suspend, no playing object !\n");
            return;
        }
        p->suspended_object = p->playing_object;
        p->suspended_pc     = p->playing_pc;

        p->playing_object = NULL;

    } else {

        if (p->playing_object) {
            BD_DEBUG(DBG_HDMV|DBG_CRIT, "_suspend_object: Movie object tries to suspend, also playing object present !\n");
            return;
        }

        p->suspended_object = p->object;
        p->suspended_pc     = p->pc;

    }

    p->object = NULL;

    _free_ig_object(p);
}

static int _resume_object(HDMV_VM *p, int psr_restore)
{
    if (!p->suspended_object) {
        BD_DEBUG(DBG_HDMV|DBG_CRIT, "_resume_object: no suspended object!\n");
        return -1;
    }

    p->object = NULL;
    p->playing_object = NULL;
    _free_ig_object(p);

    if (psr_restore) {
        /* check if suspended in play_pl */
        if (_suspended_at_play_pl(p)) {
            BD_DEBUG(DBG_HDMV, "resuming playlist playback\n");
            p->playing_object = p->suspended_object;
            p->playing_pc     = p->suspended_pc;
            p->suspended_object = NULL;
            bd_psr_restore_state(p->regs);

            return 0;
        }
        bd_psr_restore_state(p->regs);
    }

    p->object = p->suspended_object;
    p->pc     = p->suspended_pc + 1;

    p->suspended_object = NULL;

    BD_DEBUG(DBG_HDMV, "resuming object %p at %d\n", p->object, p->pc);

    _queue_event(p, HDMV_EVENT_PLAY_STOP, 0);

    return 0;
}


/*
 * branching
 */

static int _is_valid_title(HDMV_VM *p, int title)
{
    if (title == 0 || title == 0xffff) {
        INDX_PLAY_ITEM *pi = (!title) ? &p->indx->top_menu : &p->indx->first_play;

        if (pi->object_type == indx_object_type_hdmv &&  pi->hdmv.id_ref == 0xffff) {
            /* no top menu or first play title (5.2.3.3) */
            return 0;
        }
        return 1;
    }

    return title > 0 && title <= p->indx->num_titles;
}

static int _jump_object(HDMV_VM *p, int object)
{
    if (object < 0 || object >= p->movie_objects->num_objects) {
        BD_DEBUG(DBG_HDMV|DBG_CRIT, "_jump_object(): invalid object %d\n", object);
        return -1;
    }

    BD_DEBUG(DBG_HDMV, "_jump_object(): jumping to object %d\n", object);

    _queue_event(p, HDMV_EVENT_PLAY_STOP, 0);

    _free_ig_object(p);

    p->playing_object = NULL;

    p->pc     = 0;
    p->object = &p->movie_objects->objects[object];

    /* suspended object is not discarded */

    return 0;
}

static int _jump_title(HDMV_VM *p, int title)
{
    if (_is_valid_title(p, title)) {
        BD_DEBUG(DBG_HDMV, "_jump_title(%d)\n", title);

        /* discard suspended object */
        p->suspended_object = NULL;
        p->playing_object = NULL;
        bd_psr_reset_backup_registers(p->regs);

        _queue_event(p, HDMV_EVENT_TITLE, title);
        return 0;
    }

    BD_DEBUG(DBG_HDMV|DBG_CRIT, "_jump_title(%d): invalid title number\n", title);

    return -1;
}

static int _call_object(HDMV_VM *p, int object)
{
    BD_DEBUG(DBG_HDMV, "_call_object(%d)\n", object);

    _queue_event(p, HDMV_EVENT_PLAY_STOP, 0);
    _suspend_object(p, 1);

    return _jump_object(p, object);
}

static int _call_title(HDMV_VM *p, int title)
{
    if (_is_valid_title(p, title)) {
        BD_DEBUG(DBG_HDMV, "_call_title(%d)\n", title);

        _suspend_object(p, 1);

        _queue_event(p, HDMV_EVENT_TITLE, title);

        return 0;
    }

    BD_DEBUG(DBG_HDMV|DBG_CRIT, "_call_title(%d): invalid title number\n", title);

    return -1;
}

/*
 * playback control
 */

static int _play_at(HDMV_VM *p, int playlist, int playitem, int playmark)
{
    if (p->ig_object && playlist >= 0) {
        BD_DEBUG(DBG_HDMV, "play_at(list %d, item %d, mark %d): "
              "playlist change not allowed in interactive composition\n",
              playlist, playitem, playmark);
        return -1;
    }

    if (!p->ig_object && playlist < 0) {
        BD_DEBUG(DBG_HDMV, "play_at(list %d, item %d, mark %d): "
              "playlist not given in movie object (link commands not allowed)\n",
              playlist, playitem, playmark);
        return -1;
    }

    BD_DEBUG(DBG_HDMV, "play_at(list %d, item %d, mark %d)\n",
          playlist, playitem, playmark);

    if (playlist >= 0) {
        _queue_event(p, HDMV_EVENT_PLAY_PL, playlist);
        _suspend_for_play_pl(p);
    }

    if (playitem >= 0) {
        _queue_event(p, HDMV_EVENT_PLAY_PI, playitem);
    }

    if (playmark >= 0) {
        _queue_event(p, HDMV_EVENT_PLAY_PM, playmark);
    }

    return 0;
}

static int _play_stop(HDMV_VM *p)
{
    if (!p->ig_object) {
        BD_DEBUG(DBG_HDMV, "_play_stop() not allowed in movie object\n");
        return -1;
    }

    BD_DEBUG(DBG_HDMV, "_play_stop()\n");
    _queue_event(p, HDMV_EVENT_PLAY_STOP, 0);

    return 0;
}

/*
 * SET/SYSTEM setstream
 */

static void _set_stream(HDMV_VM *p, uint32_t dst, uint32_t src)
{
    BD_DEBUG(DBG_HDMV, "_set_stream(0x%x, 0x%x)\n", dst, src);

    /* primary audio stream */
    if (dst & 0x80000000) {
        bd_psr_write(p->regs, PSR_PRIMARY_AUDIO_ID, (dst >> 16) & 0xfff);
    }

    /* IG stream */
    if (src & 0x80000000) {
        bd_psr_write(p->regs, PSR_IG_STREAM_ID, (src >> 16) & 0xff);
    }

    /* angle number */
    if (src & 0x8000) {
        bd_psr_write(p->regs, PSR_ANGLE_NUMBER, src & 0xff);
    }

    /* PSR2 */

    bd_psr_lock(p->regs);

    uint32_t psr2 = bd_psr_read(p->regs, PSR_PG_STREAM);

    /* PG TextST stream number */
    if (dst & 0x8000) {
        uint32_t text_st_num = dst & 0xfff;
        psr2 = text_st_num | (psr2 & 0xfffff000);
    }

    /* Update PG TextST stream display flag */
    uint32_t disp_s_flag = (dst & 0x4000) << 17;
    psr2 = disp_s_flag | (psr2 & 0x7fffffff);

    bd_psr_write(p->regs, PSR_PG_STREAM, psr2);

    bd_psr_unlock(p->regs);
}

static void _set_sec_stream(HDMV_VM *p, uint32_t dst, uint32_t src)
{
    BD_DEBUG(DBG_HDMV, "_set_sec_stream(0x%x, 0x%x)\n", dst, src);

    uint32_t disp_v_flag   = (dst >> 30) & 1;
    uint32_t disp_a_flag   = (src >> 30) & 1;
    uint32_t text_st_flags = (src >> 13) & 3;

    /* PSR14 */

    bd_psr_lock(p->regs);

    uint32_t psr14 = bd_psr_read(p->regs, PSR_SECONDARY_AUDIO_VIDEO);

    /* secondary video */
    if (dst & 0x80000000) {
      uint32_t sec_video = dst & 0xff;
      psr14 = (sec_video << 8) | (psr14 & 0xffff00ff);
    }

    /* secondary video size */
    if (dst & 0x00800000) {
      uint32_t video_size = (dst >> 16) & 0xf;
      psr14 = (video_size << 24) | (psr14 & 0xf0ffffff);
    }

    /* secondary audio */
    if (src & 0x80000000) {
      uint32_t sec_audio = (src >> 16) & 0xff;
      psr14 = sec_audio | (psr14 & 0xffffff00);
    }

    psr14 = (disp_v_flag << 31) | (psr14 & 0x7fffffff);
    psr14 = (disp_a_flag << 30) | (psr14 & 0xbfffffff);

    bd_psr_write(p->regs, PSR_SECONDARY_AUDIO_VIDEO, psr14);

    /* PSR2 */

    uint32_t psr2  = bd_psr_read(p->regs, PSR_PG_STREAM);

    /* PiP PG TextST stream */
    if (src & 0x8000) {
        uint32_t stream = src & 0xfff;
        psr2 = (stream << 16) | (psr2 & 0xf000ffff);
    }

    psr2 = (text_st_flags << 30) | (psr2 & 0x3fffffff);

    bd_psr_write(p->regs, PSR_PG_STREAM, psr2);

    bd_psr_unlock(p->regs);
}

/*
 * SET/SYSTEM navigation control
 */

static void _set_button_page(HDMV_VM *p, uint32_t dst, uint32_t src)
{
    if (p->ig_object) {
        uint32_t param;
        param =  (src & 0xc0000000) |        /* page and effects flags */
                ((dst & 0x80000000) >> 2) |  /* button flag */
                ((src & 0x000000ff) << 16) | /* page id */
                 (dst & 0x0000ffff);         /* button id */

         _queue_event(p, HDMV_EVENT_SET_BUTTON_PAGE, param);

         /* terminate */
         p->pc = 1 << 17;

        return;
    }

    /* selected button */
    if (dst & 0x80000000) {
        bd_psr_write(p->regs, PSR_SELECTED_BUTTON_ID, dst & 0xffff);
    }

    /* active page */
    if (src & 0x80000000) {
        bd_psr_write(p->regs, PSR_MENU_PAGE_ID, src & 0xff);
    }
}

static void _enable_button(HDMV_VM *p, uint32_t dst, int enable)
{
    /* not valid in movie objects */
    if (p->ig_object) {
        if (enable) {
            _queue_event(p, HDMV_EVENT_ENABLE_BUTTON,  dst);
        } else {
            _queue_event(p, HDMV_EVENT_DISABLE_BUTTON, dst);
        }
    }
}

static void _set_still_mode(HDMV_VM *p, int enable)
{
    /* not valid in movie objects */
    if (p->ig_object) {
        _queue_event(p, HDMV_EVENT_STILL, enable);
    }
}

static void _popup_off(HDMV_VM *p)
{
    /* not valid in movie objects */
    if (p->ig_object) {
        _queue_event(p, HDMV_EVENT_POPUP_OFF, 1);
    }
}

/*
 * navigation timer
 */

static void _set_nv_timer(HDMV_VM *p, uint32_t dst, uint32_t src)
{
  uint32_t mobj_id = dst & 0xffff;
  uint32_t timeout = src & 0xffff;

  if (!timeout) {
    /* cancel timer */
    p->nv_timer.time = 0;

    bd_psr_write(p->regs, PSR_NAV_TIMER, 0);

    return;
  }

  /* validate params */
  if (mobj_id >= p->movie_objects->num_objects) {
      BD_DEBUG(DBG_HDMV|DBG_CRIT, "_set_nv_timer(): invalid object id (%d) !\n", mobj_id);
      return;
  }
  if (timeout > 300) {
      BD_DEBUG(DBG_HDMV|DBG_CRIT, "_set_nv_timer(): invalid timeout (%d) !\n", timeout);
      return;
  }

  /* set expiration time */
  p->nv_timer.time = time(NULL);
  p->nv_timer.time += timeout;

  p->nv_timer.mobj_id = mobj_id;

  bd_psr_write(p->regs, PSR_NAV_TIMER, timeout);
}

/* Unused function.
 * Commenting out to disable "‘_check_nv_timer’ defined but not used" warning
static int _check_nv_timer(HDMV_VM *p)
{
    if (p->nv_timer.time > 0) {
        time_t now = time(NULL);

        if (now >= p->nv_timer.time) {
            BD_DEBUG(DBG_HDMV, "navigation timer expired, jumping to object %d\n", p->nv_timer.mobj_id);

            bd_psr_write(p->regs, PSR_NAV_TIMER, 0);

            p->nv_timer.time = 0;
            _jump_object(p, p->nv_timer.mobj_id);

            return 0;
        }

        bd_psr_write(p->regs, PSR_NAV_TIMER, (p->nv_timer.time - now));
    }

    return -1;
}
*/

/*
 * trace
 */

static void _hdmv_trace_cmd(int pc, MOBJ_CMD *cmd)
{
    if (bd_get_debug_mask() & DBG_HDMV) {
        char buf[384], *dst = buf;

        dst += sprintf(dst, "%04d:  ", pc);

        dst += mobj_sprint_cmd(dst, cmd);

        BD_DEBUG(DBG_HDMV, "%s\n", buf);
    }
}

static void _hdmv_trace_res(uint32_t new_src, uint32_t new_dst, uint32_t orig_src, uint32_t orig_dst)
{
    if (bd_get_debug_mask() & DBG_HDMV) {

        if (new_dst != orig_dst || new_src != orig_src) {
            char buf[384], *dst = buf;

            dst += sprintf(dst, "    :  [");
            if (new_dst != orig_dst) {
                dst += sprintf(dst, " dst 0x%x <== 0x%x ", orig_dst, new_dst);
            }
            if (new_src != orig_src) {
                dst += sprintf(dst, " src 0x%x <== 0x%x ", orig_src, new_src);
            }
            dst += sprintf(dst, "]");

            BD_DEBUG(DBG_HDMV, "%s\n", buf);
        }
    }
}

/*
 * interpreter
 */

/*
 * tools
 */

#define SWAP_u32(a, b) do { uint32_t tmp = a; a = b; b = tmp; } while(0)

static inline uint32_t RAND_u32(uint32_t range)
{
  return range > 0 ? rand() % range + 1 : 0;
}

static inline uint32_t ADD_u32(uint32_t a, uint32_t b)
{
  /* overflow -> saturate */
  uint64_t result = (uint64_t)a + b;
  return result < 0xffffffff ? result : 0xffffffff;
}

static inline uint32_t MUL_u32(uint32_t a, uint32_t b)
{
  /* overflow -> saturate */
  uint64_t result = (uint64_t)a * b;
  return result < 0xffffffff ? result : 0xffffffff;
}

/*
 * _hdmv_step()
 *  - execute next instruction from current program
 */
static int _hdmv_step(HDMV_VM *p)
{
    MOBJ_CMD  *cmd  = &p->object->cmds[p->pc];
    HDMV_INSN *insn = &cmd->insn;
    uint32_t   src  = 0;
    uint32_t   dst  = 0;
    int        inc_pc = 1;

    /* fetch operand values */
    _fetch_operands(p, cmd, &dst, &src);

    /* trace */
    _hdmv_trace_cmd(p->pc, cmd);

    /* execute */
    switch (insn->grp) {
        case INSN_GROUP_BRANCH:
            switch (insn->sub_grp) {
                case BRANCH_GOTO:
                    if (insn->op_cnt > 1) {
                        BD_DEBUG(DBG_HDMV|DBG_CRIT, "[too many operands in BRANCH/GOTO opcode 0x%08x] ", *(uint32_t*)insn);
                    }
                    switch (insn->branch_opt) {
                        case INSN_NOP:                      break;
                        case INSN_GOTO:  p->pc   = dst - 1; break;
                        case INSN_BREAK: p->pc   = 1 << 17; break;
                        default:
                            BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown BRANCH/GOTO option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }
                    break;
                case BRANCH_JUMP:
                    if (insn->op_cnt > 1) {
                        BD_DEBUG(DBG_HDMV|DBG_CRIT, "[too many operands in BRANCH/JUMP opcode 0x%08x] ", *(uint32_t*)insn);
                    }
                    switch (insn->branch_opt) {
                        case INSN_JUMP_TITLE:  _jump_title(p, dst); break;
                        case INSN_CALL_TITLE:  _call_title(p, dst); break;
                        case INSN_RESUME:      _resume_object(p, 1);   break;
                        case INSN_JUMP_OBJECT: if (!_jump_object(p, dst)) { inc_pc = 0; } break;
                        case INSN_CALL_OBJECT: if (!_call_object(p, dst)) { inc_pc = 0; } break;
                        default:
                            BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown BRANCH/JUMP option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }
                    break;
                case BRANCH_PLAY:
                    switch (insn->branch_opt) {
                        case INSN_PLAY_PL:      _play_at(p, dst,  -1,  -1); break;
                        case INSN_PLAY_PL_PI:   _play_at(p, dst, src,  -1); break;
                        case INSN_PLAY_PL_PM:   _play_at(p, dst,  -1, src); break;
                        case INSN_TERMINATE_PL: _play_stop(p);              break;
                        case INSN_LINK_PI:      _play_at(p,  -1, dst,  -1); break;
                        case INSN_LINK_MK:      _play_at(p,  -1,  -1, dst); break;
                        default:
                            BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown BRANCH/PLAY option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }
                    break;

                default:
                    BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown BRANCH subgroup in opcode 0x%08x] ", *(uint32_t*)insn);
                    break;
            }
            break; /* INSN_GROUP_BRANCH */

        case INSN_GROUP_CMP:
            if (insn->op_cnt < 2) {
                BD_DEBUG(DBG_HDMV|DBG_CRIT, "missing operand in BRANCH/JUMP opcode 0x%08x] ", *(uint32_t*)insn);
            }
            switch (insn->cmp_opt) {
                case INSN_BC: p->pc += !!(dst & ~src); break;
                case INSN_EQ: p->pc += !(dst == src); break;
                case INSN_NE: p->pc += !(dst != src); break;
                case INSN_GE: p->pc += !(dst >= src); break;
                case INSN_GT: p->pc += !(dst >  src); break;
                case INSN_LE: p->pc += !(dst <= src); break;
                case INSN_LT: p->pc += !(dst <  src); break;
                default:
                    BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown COMPARE option in opcode 0x%08x] ", *(uint32_t*)insn);
                    break;
            }
            break; /* INSN_GROUP_CMP */

        case INSN_GROUP_SET:
            switch (insn->sub_grp) {
                case SET_SET: {
                    uint32_t src0 = src;
                    uint32_t dst0 = dst;

                    if (insn->op_cnt < 2) {
                        BD_DEBUG(DBG_HDMV|DBG_CRIT, "missing operand in SET/SET opcode 0x%08x] ", *(uint32_t*)insn);
                    }
                    switch (insn->set_opt) {
                        case INSN_MOVE:   dst  = src;         break;
                        case INSN_SWAP:   SWAP_u32(src, dst);   break;
                        case INSN_SUB:    dst  = dst > src ? dst - src :          0; break;
                        case INSN_DIV:    dst  = src > 0   ? dst / src : 0xffffffff; break;
                        case INSN_MOD:    dst  = src > 0   ? dst % src : 0xffffffff; break;
                        case INSN_ADD:    dst  = ADD_u32(src, dst);  break;
                        case INSN_MUL:    dst  = MUL_u32(dst, src);  break;
                        case INSN_RND:    dst  = RAND_u32(src);      break;
                        case INSN_AND:    dst &= src;         break;
                        case INSN_OR:     dst |= src;         break;
                        case INSN_XOR:    dst ^= src;         break;
                        case INSN_BITSET: dst |=  (1 << src); break;
                        case INSN_BITCLR: dst &= ~(1 << src); break;
                        case INSN_SHL:    dst <<= src;        break;
                        case INSN_SHR:    dst >>= src;        break;
                        default:
                            BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown SET option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }

                    /* store result(s) */
                    if (dst != dst0 || src != src0) {

                        _hdmv_trace_res(src, dst, src0, dst0);

                        _store_result(p, cmd, src, dst, src0, dst0);
                    }
                    break;
                }
                case SET_SETSYSTEM:
                    switch (insn->set_opt) {
                        case INSN_SET_STREAM:      _set_stream     (p, dst, src); break;
                        case INSN_SET_SEC_STREAM:  _set_sec_stream (p, dst, src); break;
                        case INSN_SET_NV_TIMER:    _set_nv_timer   (p, dst, src); break;
                        case INSN_SET_BUTTON_PAGE: _set_button_page(p, dst, src); break;
                        case INSN_ENABLE_BUTTON:   _enable_button  (p, dst,   1); break;
                        case INSN_DISABLE_BUTTON:  _enable_button  (p, dst,   0); break;
                        case INSN_POPUP_OFF:       _popup_off      (p);           break;
                        case INSN_STILL_ON:        _set_still_mode (p,   1);      break;
                        case INSN_STILL_OFF:       _set_still_mode (p,   0);      break;
                        default:
                            BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown SETSYSTEM option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }
                    break;
                default:
                    BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown SET subgroup in opcode 0x%08x] ", *(uint32_t*)insn);
                    break;
            }
            break; /* INSN_GROUP_SET */

        default:
            BD_DEBUG(DBG_HDMV|DBG_CRIT, "[unknown group in opcode 0x%08x] ", *(uint32_t*)insn);
            break;
    }

    /* inc program counter to next instruction */
    p->pc += inc_pc;

    return 0;
}

/*
 * interface
 */

int hdmv_vm_select_object(HDMV_VM *p, int object)
{
    int result;
    bd_mutex_lock(&p->mutex);

    result = _jump_object(p, object);

    bd_mutex_unlock(&p->mutex);
    return result;
}

int hdmv_vm_set_object(HDMV_VM *p, int num_nav_cmds, void *nav_cmds)
{
    int result = -1;
    bd_mutex_lock(&p->mutex);

    p->object = NULL;

    _free_ig_object(p);

    if (nav_cmds && num_nav_cmds > 0) {
        MOBJ_OBJECT *ig_object = calloc(1, sizeof(MOBJ_OBJECT));
        ig_object->num_cmds = num_nav_cmds;
        ig_object->cmds     = calloc(num_nav_cmds, sizeof(MOBJ_CMD));
        memcpy(ig_object->cmds, nav_cmds, num_nav_cmds * sizeof(MOBJ_CMD));

        p->pc        = 0;
        p->ig_object = ig_object;
        p->object    = ig_object;

        result = 0;
    }

    bd_mutex_unlock(&p->mutex);

    return result;
}

int hdmv_vm_get_event(HDMV_VM *p, HDMV_EVENT *ev)
{
    int result;
    bd_mutex_lock(&p->mutex);

    result = _get_event(p, ev);

    bd_mutex_unlock(&p->mutex);
    return result;
}

int hdmv_vm_running(HDMV_VM *p)
{
    int result;
    bd_mutex_lock(&p->mutex);

    result = !!p->object;

    bd_mutex_unlock(&p->mutex);
    return result;
}

uint32_t hdmv_vm_get_uo_mask(HDMV_VM *p)
{
    uint32_t     mask = 0;
    MOBJ_OBJECT *o    = NULL;

    bd_mutex_lock(&p->mutex);

    if ((o = p->object ? p->object : (p->playing_object ? p->playing_object : p->suspended_object))) {
        mask |= o->menu_call_mask;
        mask |= o->title_search_mask << 1;
    }

    bd_mutex_unlock(&p->mutex);
    return mask;
}

int hdmv_vm_resume(HDMV_VM *p)
{
    int result;
    bd_mutex_lock(&p->mutex);

    result = _resume_from_play_pl(p);

    bd_mutex_unlock(&p->mutex);
    return result;
}

int hdmv_vm_suspend_pl(HDMV_VM *p)
{
    int result = -1;
    bd_mutex_lock(&p->mutex);

    if (p->object || p->ig_object) {
        BD_DEBUG(DBG_HDMV, "hdmv_vm_suspend_pl(): HDMV VM is still running\n");

    } else if (!p->playing_object) {
        BD_DEBUG(DBG_HDMV, "hdmv_vm_suspend_pl(): No playing object\n");

    } else if (!p->playing_object->resume_intention_flag) {
        BD_DEBUG(DBG_HDMV, "hdmv_vm_suspend_pl(): no resume intention flag\n");

        p->playing_object = NULL;
        result = 0;

    } else {
        p->suspended_object = p->playing_object;
        p->suspended_pc     = p->playing_pc;

        p->playing_object = NULL;

        bd_psr_save_state(p->regs);
        result = 0;
    }

    bd_mutex_unlock(&p->mutex);
    return result;
}

/* terminate program after MAX_LOOP instructions */
#define MAX_LOOP 1000000

static int _vm_run(HDMV_VM *p, HDMV_EVENT *ev)
{
    int max_loop = MAX_LOOP;

    /* pending events ? */
    if (!_get_event(p, ev)) {
        return 0;
    }

    /* valid program ? */
    if (!p->object) {
        BD_DEBUG(DBG_HDMV|DBG_CRIT, "hdmv_vm_run(): no object selected\n");
        return -1;
    }

    while (--max_loop > 0) {

        /* suspended ? */
        if (!p->object) {
            BD_DEBUG(DBG_HDMV, "hdmv_vm_run(): object suspended\n");
            _get_event(p, ev);
            return 0;
        }

        /* terminated ? */
        if (p->pc >= p->object->num_cmds) {
            BD_DEBUG(DBG_HDMV, "terminated with PC=%d\n", p->pc);
            p->object = NULL;
            ev->event = HDMV_EVENT_END;

            if (p->ig_object) {
                ev->event = HDMV_EVENT_IG_END;
                _free_ig_object(p);
            }

            return 0;
        }

        /* next instruction */
        if (_hdmv_step(p) < 0) {
            p->object = NULL;
            return -1;
        }

        /* events ? */
        if (!_get_event(p, ev)) {
            return 0;
        }
    }

    BD_DEBUG(DBG_HDMV|DBG_CRIT, "hdmv_vm: infinite program ? terminated after %d instructions.\n", MAX_LOOP);
    p->object = NULL;
    return -1;
}

int hdmv_vm_run(HDMV_VM *p, HDMV_EVENT *ev)
{
    int result;
    bd_mutex_lock(&p->mutex);

    result = _vm_run(p, ev);

    bd_mutex_unlock(&p->mutex);
    return result;
}
