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

#include "util/macro.h"
#include "util/strutl.h"
#include "util/logging.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>


#define SWAP32(a,b) { uint32_t tmp = a; a = b; b = tmp; }

struct hdmv_vm_s {
    /* state */
    uint32_t       pc;            /* program counter */
    BD_REGISTERS  *regs;          /* player registers */
    MOBJ_OBJECT   *object;        /* currently running object code */

    HDMV_EVENT     event[5];      /* pending events to return */

    /* movie objects */
    MOBJ_OBJECTS  *movie_objects; /* disc movie objects */
    MOBJ_OBJECT   *ig_object;     /* current object from IG stream */

    /* suspended object */
    MOBJ_OBJECT *suspended_object;
    int          suspended_pc;
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
        DEBUG(DBG_HDMV, "_store_reg(): invalid register 0x%x\n", reg);
        return -1;
    }

    if (reg & PSR_FLAG) {
        return bd_psr_write(p->regs, reg & 0x7f, val);
    }  else {
        return bd_gpr_write(p->regs, reg, val);
    }
}

static uint32_t _read_reg(HDMV_VM *p, uint32_t reg)
{
    if (!_is_valid_reg(reg)) {
        DEBUG(DBG_HDMV, "_read_reg(): invalid register 0x%x\n", reg);
        return 0;
    }

    if (reg & PSR_FLAG) {
        return bd_psr_read(p->regs, reg & 0x7f);
    } else {
        return bd_gpr_read(p->regs, reg);
    }
}

static int _store_result(HDMV_VM *p, MOBJ_CMD *cmd, uint32_t src, uint32_t dst, uint32_t src0, uint32_t dst0)
{
    int ret = 0;

    /* store result to destination register(s) */
    if (dst != dst0) {
        if (cmd->insn.imm_op1) {
            DEBUG(DBG_HDMV|DBG_CRIT, "ERROR: storing to imm ! ");
            return -1;
        }
        ret = _store_reg(p, cmd->dst, dst);
    }

    if (src != src0) {
        if (cmd->insn.imm_op1) {
            DEBUG(DBG_HDMV|DBG_CRIT, "ERROR: storing to imm ! ");
            return -1;
        }
        ret += _store_reg(p, cmd->src, src);
    }

    return ret;
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

static int _queue_event(HDMV_VM *p, HDMV_EVENT ev)
{
    unsigned i;
    for (i = 0; i < sizeof(p->event) / sizeof(p->event[0]) - 1; i++) {
        if (p->event[i].event == HDMV_EVENT_NONE) {
            p->event[i] = ev;
            return 0;
        }
    }

    DEBUG(DBG_HDMV|DBG_CRIT, "_queue_event(%d, %d): queue overflow !\n", ev.event, ev.param);
    return -1;
}

/*
 * vm init
 */

HDMV_VM *hdmv_vm_init(const char *disc_root, BD_REGISTERS *regs)
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

    return  p;
}

void hdmv_vm_free(HDMV_VM *p)
{
    mobj_free(p->movie_objects);

    X_FREE(p);
}

/*
 * suspend/resume ("function call")
 */

static void _suspend_object(HDMV_VM *p)
{
    DEBUG(DBG_HDMV, "_suspend_object()\n");

    if (p->suspended_object) {
        DEBUG(DBG_HDMV, "_suspend_object: object already suspended !\n");
        // [execute the call, discard old suspended object (10.2.4.2.2)].
    }

    bd_psr_save_state(p->regs);

    p->suspended_object = p->object;
    p->suspended_pc     = p->pc;

}

static int _resume_object(HDMV_VM *p)
{
    if (!p->suspended_object) {
        DEBUG(DBG_HDMV|DBG_CRIT, "_resume_object: no suspended object!\n");
        return -1;
    }

    p->object = p->suspended_object;
    p->pc     = p->suspended_pc;

    bd_psr_restore_state(p->regs);

    DEBUG(DBG_HDMV, "resuming object %p at %d\n", p->object, p->pc + 1);

    p->suspended_object = NULL;

    return 0;
}


/*
 * branching
 */

static int _jump_object(HDMV_VM *p, int object)
{
    if (object < 0 || object > p->movie_objects->num_objects) {
        DEBUG(DBG_HDMV|DBG_CRIT, "_jump_object(): invalid object %d\n", object);
        return -1;
    }

    DEBUG(DBG_HDMV, "_jump_object(): jumping to object %d\n", object);

    p->pc     = 0;
    p->object = &p->movie_objects->objects[object];

    return 0;
}

static int _jump_title(HDMV_VM *p, int title)
{
    DEBUG(DBG_HDMV, "_jump_title(%d)\n", title);

    if (p->suspended_object) {
        /* discard suspended object */
        p->suspended_object = NULL;
        bd_psr_restore_state(p->regs);
    }

    if (title >= 0) {
        _queue_event(p, (HDMV_EVENT){HDMV_EVENT_TITLE, title});
        return 0;
    }

    return -1;
}

static int _call_object(HDMV_VM *p, int object)
{
    DEBUG(DBG_HDMV, "_call_object(%d)\n", object);

    _suspend_object(p);

    return _jump_object(p, object);
}

static int _call_title(HDMV_VM *p, int title)
{
    DEBUG(DBG_HDMV, "_call_title(%d)\n", title);

    _suspend_object(p);

    if (title >= 0) {
        _queue_event(p, (HDMV_EVENT){HDMV_EVENT_TITLE, title});
        return 0;
    }

    return -1;
}

/*
 * playback control
 */

static int _play_at(HDMV_VM *p, int playlist, int playitem, int playmark)
{
    if (playlist >= 0) {
        DEBUG(DBG_HDMV, "open playlist %d\n", playlist);
        _queue_event(p, (HDMV_EVENT){HDMV_EVENT_PLAY_PL, playlist});
    }

    if (playitem >= 0) {
        DEBUG(DBG_HDMV, "seek to playitem %d\n", playitem);
        _queue_event(p, (HDMV_EVENT){HDMV_EVENT_PLAY_PI, playitem});
    }

    if (playmark >= 0) {
        DEBUG(DBG_HDMV, "seek to playmark %d\n", playmark);
        _queue_event(p, (HDMV_EVENT){HDMV_EVENT_PLAY_PM, playmark});
    }

    DEBUG(DBG_HDMV|DBG_CRIT, "play_at: list %d, item %d, mark %d\n",
          playlist, playitem, playmark);

    return 0;
}

static int _play_stop(HDMV_VM *p)
{
    DEBUG(DBG_HDMV, "_play_stop()\n");
    _queue_event(p, (HDMV_EVENT){HDMV_EVENT_PLAY_STOP, -1});
    return 0;
}

/*
 * trace
 */

static void _hdmv_trace_cmd(int pc, MOBJ_CMD *cmd, uint32_t orig_src, uint32_t orig_dst, uint32_t new_src, uint32_t new_dst)
{
    if (bd_get_debug_mask() & DBG_HDMV) {
        char buf[384], *dst = buf;

        dst += sprintf(dst, "%04d:  ", pc);

        dst += mobj_sprint_cmd(dst, cmd);

        if (new_dst != orig_dst || new_src != orig_src) {
            dst += sprintf(dst, " [");
            if (new_dst != orig_dst) {
                dst += sprintf(dst, " dst 0x%x <== 0x%x ", orig_dst, new_dst);
            }
            if (new_src != orig_src) {
                dst += sprintf(dst, " src 0x%x <== 0x%x ", orig_src, new_src);
            }
            dst += sprintf(dst, "] ");
        }

        DEBUG(DBG_HDMV, "%s\n", buf);
    }
}

/*
 * interpreter
 */

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
    uint32_t   src0 = 0;
    uint32_t   dst0 = 0;
    uint32_t   orig_pc = p->pc;
    int        inc_pc = 1;

    /* fetch operand values */
    if (insn->op_cnt > 0) {
        if (!insn->imm_op1) {
            dst = _read_reg(p, cmd->dst);
        } else {
            dst = cmd->dst;
        }
    }
    if (insn->op_cnt > 1) {
        if (!insn->imm_op2) {
            src = _read_reg(p, cmd->src);
        } else {
            src = cmd->src;
        }
    }
    src0 = src;
    dst0 = dst;

    /* execute */
    switch (insn->grp) {
        case INSN_GROUP_BRANCH:
            switch (insn->sub_grp) {
                case BRANCH_GOTO:
                    if (insn->op_cnt > 1) {
                        DEBUG(DBG_HDMV|DBG_CRIT, "[too many operands in BRANCH/GOTO opcode 0x%08x] ", *(uint32_t*)insn);
                    }
                    switch (insn->branch_opt) {
                        case INSN_NOP:                      break;
                        case INSN_GOTO:  p->pc   = dst - 1; break;
                        case INSN_BREAK: p->pc   = 1 << 17; break;
                        default:
                            DEBUG(DBG_HDMV|DBG_CRIT, "[unknown BRANCH/GOTO option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }
                    break;
                case BRANCH_JUMP:
                    if (insn->op_cnt > 1) {
                        DEBUG(DBG_HDMV|DBG_CRIT, "[too many operands in BRANCH/JUMP opcode 0x%08x] ", *(uint32_t*)insn);
                    }
                    switch (insn->branch_opt) {
                        case INSN_JUMP_TITLE:  _jump_title(p, dst); break;
                        case INSN_CALL_TITLE:  _call_title(p, dst); break;
                        case INSN_RESUME:      _resume_object(p);   break;
                        case INSN_JUMP_OBJECT: if (!_jump_object(p, dst)) { inc_pc = 0; } break;
                        case INSN_CALL_OBJECT: if (!_call_object(p, dst)) { inc_pc = 0; } break;
                        default:
                            DEBUG(DBG_HDMV|DBG_CRIT, "[unknown BRANCH/JUMP option in opcode 0x%08x] ", *(uint32_t*)insn);
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
                            DEBUG(DBG_HDMV|DBG_CRIT, "[unknown BRANCH/PLAY option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }
                    break;

                default:
                    DEBUG(DBG_HDMV|DBG_CRIT, "[unknown BRANCH subgroup in opcode 0x%08x] ", *(uint32_t*)insn);
                    break;
            }
            break; /* INSN_GROUP_BRANCH */

        case INSN_GROUP_CMP:
            if (insn->op_cnt < 2) {
                DEBUG(DBG_HDMV|DBG_CRIT, "missing operand in BRANCH/JUMP opcode 0x%08x] ", *(uint32_t*)insn);
            }
            switch (insn->cmp_opt) {
                case INSN_BC: p->pc += !(dst == src); break;
                case INSN_EQ: p->pc += !(dst == src); break;
                case INSN_NE: p->pc += !(dst != src); break;
                case INSN_GE: p->pc += !(dst >= src); break;
                case INSN_GT: p->pc += !(dst >  src); break;
                case INSN_LE: p->pc += !(dst <= src); break;
                case INSN_LT: p->pc += !(dst <  src); break;
                default:
                    DEBUG(DBG_HDMV|DBG_CRIT, "[unknown COMPARE option in opcode 0x%08x] ", *(uint32_t*)insn);
                    break;
            }
            break; /* INSN_GROUP_CMP */

        case INSN_GROUP_SET:
            switch (insn->sub_grp) {
                case SET_SET:
                    if (insn->op_cnt < 2) {
                        DEBUG(DBG_HDMV|DBG_CRIT, "missing operand in SET/SET opcode 0x%08x] ", *(uint32_t*)insn);
                    }
                    switch (insn->set_opt) {
                        case INSN_MOVE:   dst  = src;         break;
                        case INSN_SWAP:   SWAP32(src, dst);   break;
                        case INSN_ADD:    dst += src;         break;
                        case INSN_SUB:    dst  = dst < src ? 0         : dst - src;  break;
                        case INSN_MUL:    dst *= src;         break;
                        case INSN_DIV:    dst  = src > 0   ? dst / src : 0xffffffff; break;
                        case INSN_MOD:    dst  = src > 0   ? dst % src : 0xffffffff; break;
                        case INSN_RND:    dst = (rand() % src) + 1;                  break;
                        case INSN_AND:    dst &= src;         break;
                        case INSN_OR:     dst |= src;         break;
                        case INSN_XOR:    dst ^= src;         break;
                        case INSN_BITSET: dst |=  (1 << src); break;
                        case INSN_BITCLR: dst &= ~(1 << src); break;
                        case INSN_SHL:    dst <<= src;        break;
                        case INSN_SHR:    dst >>= src;        break;
                        default:
                            DEBUG(DBG_HDMV|DBG_CRIT, "[unknown SET option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }

                    /* store result(s) */
                    if (dst != dst0 || src != src0) {
                        _store_result(p, cmd, src, dst, src0, dst0);
                    }
                    break;
                case SET_SETSYSTEM:
                    switch (insn->set_opt) {
                        default:
                            DEBUG(DBG_HDMV|DBG_CRIT, "[unknown SETSYSTEM option in opcode 0x%08x] ", *(uint32_t*)insn);
                            break;
                    }
                    break;
                default:
                    DEBUG(DBG_HDMV|DBG_CRIT, "[unknown SET subgroup in opcode 0x%08x] ", *(uint32_t*)insn);
                    break;
            }
            break; /* INSN_GROUP_SET */

        default:
            DEBUG(DBG_HDMV|DBG_CRIT, "[unknown group in opcode 0x%08x] ", *(uint32_t*)insn);
            break;
    }

    /* trace */
    _hdmv_trace_cmd(orig_pc, cmd, src0, dst0, src, dst);

    /* inc program counter to next instruction */
    p->pc += inc_pc;

    return 0;
}

/*
 * interface
 */

int hdmv_vm_select_object(HDMV_VM *p, int object, void *ig_object)
{
    if (object >= 0) {
        if (object >= p->movie_objects->num_objects) {
            DEBUG(DBG_HDMV|DBG_CRIT, "hdmv_vm_select_program(): invalid object reference (%d) !\n", object);
            return -1;
        }
        p->pc     = 0;
        p->object = &p->movie_objects->objects[object];
    }
    if (ig_object) {
#if 0
        /* ??? */
        if (!p->ig_object && p->object) {
            _suspend_object(p);
        }
#endif
        p->pc        = 0;
        p->ig_object = ig_object;
        p->object    = ig_object;
    }

    return 0;
}

int hdmv_vm_get_event(HDMV_VM *p, HDMV_EVENT *ev)
{
    return _get_event(p, ev);
}

/* terminate program after MAX_LOOP instructions */
#define MAX_LOOP 1000000

int hdmv_vm_run(HDMV_VM *p, HDMV_EVENT *ev)
{
    int max_loop = MAX_LOOP;

    /* pending events ? */
    if (!_get_event(p, ev)) {
        return 0;
    }

    /* valid program ? */
    if (!p->object) {
        DEBUG(DBG_HDMV|DBG_CRIT, "hdmv_vm_run(): no object selected\n");
        return -1;
    }

    while (--max_loop > 0) {

        /* terminated ? */
        if (p->pc >= p->object->num_cmds) {
            DEBUG(DBG_HDMV, "terminated with PC=%d\n", p->pc);
            p->object = NULL;
            ev->event = HDMV_EVENT_END;
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

    DEBUG(DBG_HDMV|DBG_CRIT, "hdmv_vm: infinite program ? terminated after %d instructions.\n", MAX_LOOP);
    p->object = NULL;
    return -1;
}


