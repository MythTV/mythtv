/*
 * This file is part of libbluray
 * Copyright (C) 2009-2012  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "mobj_print.h"

#include "mobj_data.h"
#include "hdmv_insn.h"

#include <stdio.h>

static const char * const psr_info[128] = {
    "/*     PSR0:  Interactive graphics stream number */",
    "/*     PSR1:  Primary audio stream number */",
    "/*     PSR2:  PG TextST stream number and PiP PG stream number */",
    "/*     PSR3:  Angle number */",
    "/*     PSR4:  Title number */",
    "/*     PSR5:  Chapter number */",
    "/*     PSR6:  PlayList ID */",
    "/*     PSR7:  PlayItem ID */",
    "/*     PSR8:  Presentation time */",
    "/*     PSR9:  Navigation timer */",
    "/*     PSR10: Selected button ID */",
    "/*     PSR11: Page ID */",
    "/*     PSR12: User style number */",
    "/* RO: PSR13: User age */",
    "/*     PSR14: Secondary audio stream number and secondary video stream number */",
    "/* RO: PSR15: player capability for audio */",
    "/* RO: PSR16: Language code for audio */",
    "/* RO: PSR17: Language code for PG and Text subtitles */",
    "/* RO: PSR18: Menu description language code */",
    "/* RO: PSR19: Country code */",
    "/* RO: PSR20: Region code */ /* 1 - A, 2 - B, 4 - C */",
    "/* RO: PSR21: Output Mode Preference */ /* 0 - 2D, 1 - 3D */",
    "/*     PSR22: Stereoscopic status */ /* 2D / 3D */ ",
    "/* RO: PSR23: display capablity */",
    "/* RO: PSR24: 3D capability */",
    "/*     PSR25 */",
    "/*     PSR26 */",
    "/*     PSR27 */",
    "/*     PSR28 */",
    "/* RO: PSR29: player capability for video */",
    "/* RO: PSR30: player capability for text subtitle */",
    "/* RO: PSR31: Player profile and version */",
    "/*     PSR32 */",
    "/*     PSR33 */",
    "/*     PSR34 */",
    "/*     PSR35 */",
    "/*     PSR36: backup PSR4 */",
    "/*     PSR37: backup PSR5 */",
    "/*     PSR38: backup PSR6 */",
    "/*     PSR39: backup PSR7 */",
    "/*     PSR40: backup PSR8 */",
    "/*     PSR41: */",
    "/*     PSR42: backup PSR10 */",
    "/*     PSR43: backup PSR11 */",
    "/*     PSR44: backup PSR12 */",
    "/*     PSR45: */",
    "/*     PSR46: */",
    "/*     PSR47: */",
    "/* RO: PSR48: Characteristic text caps */",
    "/* RO: PSR49: Characteristic text caps */",
    "/* RO: PSR50: Characteristic text caps */",
    "/* RO: PSR51: Characteristic text caps */",
    "/* RO: PSR52: Characteristic text caps */",
    "/* RO: PSR53: Characteristic text caps */",
    "/* RO: PSR54: Characteristic text caps */",
    "/* RO: PSR55: Characteristic text caps */",
    "/* RO: PSR56: Characteristic text caps */",
    "/* RO: PSR57: Characteristic text caps */",
    "/* RO: PSR58: Characteristic text caps */",
    "/* RO: PSR59: Characteristic text caps */",
    "/* RO: PSR60: Characteristic text caps */",
    "/* RO: PSR61: Characteristic text caps */",
    /* PSR62 */ NULL,
    /* PSR63 */ NULL,
    /* PSR64 */ NULL,
    /* PSR65 */ NULL,
    /* PSR66 */ NULL,
    /* PSR67 */ NULL,
    /* PSR68 */ NULL,
    /* PSR69 */ NULL,
    /* PSR70 */ NULL,
    /* PSR71 */ NULL,
    /* PSR72 */ NULL,
    /* PSR73 */ NULL,
    /* PSR74 */ NULL,
    /* PSR75 */ NULL,
    /* PSR76 */ NULL,
    /* PSR77 */ NULL,
    /* PSR78 */ NULL,
    /* PSR79 */ NULL,
    /* PSR80 */ NULL,
    /* PSR81 */ NULL,
    /* PSR82 */ NULL,
    /* PSR83 */ NULL,
    /* PSR84 */ NULL,
    /* PSR85 */ NULL,
    /* PSR86 */ NULL,
    /* PSR87 */ NULL,
    /* PSR88 */ NULL,
    /* PSR89 */ NULL,
    /* PSR90 */ NULL,
    /* PSR91 */ NULL,
    /* PSR92 */ NULL,
    /* PSR93 */ NULL,
    /* PSR94 */ NULL,
    /* PSR95 */ NULL,
    /* PSR96 */ NULL,
    /* PSR97 */ NULL,
    /* PSR98 */ NULL,
    /* PSR99 */ NULL,
    /* PSR100 */ NULL,
    /* PSR101 */ NULL,
    "/*     PSR102: BD+ receive */",
    "/*     PSR103: BD+ send */",
    "/*     PSR104: BD+ shared */",
    /* PSR105 */ NULL,
    /* PSR106 */ NULL,
    /* PSR107 */ NULL,
    /* PSR108 */ NULL,
    /* PSR109 */ NULL,
    /* PSR110 */ NULL,
    /* PSR111 */ NULL,
    /* PSR112 */ NULL,
    /* PSR113 */ NULL,
    /* PSR114 */ NULL,
    /* PSR115 */ NULL,
    /* PSR116 */ NULL,
    /* PSR117 */ NULL,
    /* PSR118 */ NULL,
    /* PSR119 */ NULL,
    /* PSR120 */ NULL,
    /* PSR121 */ NULL,
    /* PSR122 */ NULL,
    /* PSR123 */ NULL,
    /* PSR124 */ NULL,
    /* PSR125 */ NULL,
    /* PSR126 */ NULL,
    /* PSR127 */ NULL,
};

#if 0
static const char * const insn_groups[4] = {
    "BRANCH",
    "COMPARE",
    "SET",
};

static const char * const insn_group_branch[8] = {
    "GOTO",
    "JUMP",
    "PLAY",
};

static const char * const insn_group_set[8] = {
    "SET",
    "SETSYSTEM",
};
#endif

static const char * const insn_opt_set[32] = {
    NULL,
    "move",
    "swap",
    "add",
    "sub",
    "mul",
    "div",
    "mod",
    "rnd",
    "and",
    "or",
    "xor",
    "bset",
    "bclr",
    "shl",
    "shr",
};

static const char * const insn_opt_setsys[32] = {
    NULL,
    "SET_STREAM",
    "SET_NV_TIMER",
    "SET_BUTTON_PAGE",
    "ENABLE_BUTTON",
    "DISABLE_BUTTON",
    "SET_SEC_STREAM",
    "POPUP_OFF",
    "STILL_ON",
    "STILL_OFF",
    "SET_OUTPUT_MODE",
    "SET_STREAM_SS",
    NULL,
    NULL,
    NULL,
    NULL,
    "[SETSYSTEM_0x10]",
};

static const char * const insn_opt_cmp[16] = {
    NULL,
    "bc",
    "eq",
    "ne",
    "ge",
    "gt",
    "le",
    "lt",
};

static const char * const insn_opt_goto[16] = {
    "nop",
    "goto",
    "break",
};

static const char * const insn_opt_jump[16] = {
    "JUMP_OBJECT",
    "JUMP_TITLE",
    "CALL_OBJECT",
    "CALL_TITLE",
    "RESUME"
};

static const char * const insn_opt_play[16] = {
    "PLAY_PL",
    "PLAY_PL_PI",
    "PLAY_PL_MK",
    "TERMINATE_PL",
    "LINK_PI",
    "LINK_MK"
};

static int _sprint_operand(char *buf, int imm, uint32_t op, int *psr)
{
    char *start = buf;

    if (!imm) {
        if (op & 0x80000000) {
            buf += sprintf(buf, "PSR%-3u", op & 0x7f);
            *psr = op & 0x7f;
        } else {
            buf += sprintf(buf, "r%-5u", op & 0xfff);
        }
    } else {
        if (op < 99999)
            buf += sprintf(buf, "%-6u", op);
        else
            buf += sprintf(buf, "0x%-4x", op);
    }

    return buf - start;
}

static int _sprint_operands(char *buf, MOBJ_CMD *cmd)
{
    char      *start = buf;
    HDMV_INSN *insn  = &cmd->insn;
    int psr1 = -1, psr2 = -1;

    if (insn->op_cnt > 0) {
        buf += _sprint_operand(buf, insn->imm_op1, cmd->dst, &psr1);

        if (insn->op_cnt > 1) {
            buf += sprintf(buf, ",\t");
            buf += _sprint_operand(buf, insn->imm_op2, cmd->src, &psr2);
        } else {
            buf += sprintf(buf, " \t      ");
        }
    } else {
        buf += sprintf(buf, "       \t      ");
    }

    if (psr1 >= 0 && psr1 < 128 && psr_info[psr1])
        buf += sprintf(buf, " %s", psr_info[psr1]);
    if (psr2 >= 0 && psr2 < 128 && psr2 != psr1 && psr_info[psr2])
        buf += sprintf(buf, " %s", psr_info[psr2]);

    return buf - start;
}

static int _sprint_operands_hex(char *buf, MOBJ_CMD *cmd)
{
    char      *start = buf;
    HDMV_INSN *insn  = &cmd->insn;

    if (insn->op_cnt > 0) {
        buf += sprintf(buf, "0x%-4x", cmd->dst);
    }
    if (insn->op_cnt > 1) {
        buf += sprintf(buf,  ",\t0x%-4x", cmd->src);
    }

    return buf - start;
}

static uint32_t _cmd_to_u32(HDMV_INSN *insn)
{
    union {
        HDMV_INSN insn;
        uint8_t u8[4];
    } tmp;
    tmp.insn = *insn;
    return ((unsigned)tmp.u8[0] << 24) | (tmp.u8[1] << 16) | (tmp.u8[2] << 8) | tmp.u8[3];
}

int mobj_sprint_cmd(char *buf, MOBJ_CMD *cmd)
{
    char *start = buf;
    HDMV_INSN *insn = &cmd->insn;

    buf += sprintf(buf, "%08x %08x,%08x  ", _cmd_to_u32(&cmd->insn), cmd->dst, cmd->src);

    switch(insn->grp) {
        case INSN_GROUP_BRANCH:
            switch(insn->sub_grp) {
                case BRANCH_GOTO:
                    if (insn_opt_goto[insn->branch_opt]) {
                        buf += sprintf(buf, "%-10s ", insn_opt_goto[insn->branch_opt]);
                        buf += _sprint_operands(buf, cmd);
                    } else {
                      buf += sprintf(buf, "[unknown BRANCH/GOTO option in opcode 0x%08x] ", *(uint32_t*)insn);
                    }
                    break;
            case BRANCH_JUMP:
                if (insn_opt_jump[insn->branch_opt]) {
                    buf += sprintf(buf, "%-10s ", insn_opt_jump[insn->branch_opt]);
                    buf += _sprint_operands(buf, cmd);
                } else {
                    buf += sprintf(buf, "[unknown BRANCH/JUMP option in opcode 0x%08x] ", *(uint32_t*)insn);
                }
                break;
            case BRANCH_PLAY:
                if (insn_opt_play[insn->branch_opt]) {
                    buf += sprintf(buf, "%-10s ", insn_opt_play[insn->branch_opt]);
                    buf += _sprint_operands(buf, cmd);
                } else {
                    buf += sprintf(buf, "[unknown BRANCH/PLAY option in opcode 0x%08x] ", *(uint32_t*)insn);
                }
                break;
            default:
                buf += sprintf(buf, "[unknown BRANCH subgroup in opcode 0x%08x] ", *(uint32_t*)insn);
                break;
            }
            break;

        case INSN_GROUP_CMP:
            if (insn_opt_cmp[insn->cmp_opt]) {
                buf += sprintf(buf, "%-10s ", insn_opt_cmp[insn->cmp_opt]);
                buf += _sprint_operands(buf, cmd);
            } else {
                buf += sprintf(buf, "[unknown COMPARE option in opcode 0x%08x] ", *(uint32_t*)insn);
            }
            break;

        case INSN_GROUP_SET:
            switch (insn->sub_grp) {
                case SET_SET:
                    if (insn_opt_set[insn->set_opt]) {
                        buf += sprintf(buf, "%-10s ", insn_opt_set[insn->set_opt]);
                        buf += _sprint_operands(buf, cmd);
                    } else {
                        buf += sprintf(buf, "[unknown SET option in opcode 0x%08x] ", *(uint32_t*)insn);
                    }
                    break;
            case SET_SETSYSTEM:
                if (insn_opt_setsys[insn->set_opt]) {

                    buf += sprintf(buf, "%-10s ", insn_opt_setsys[insn->set_opt]);
                    if (insn->set_opt == INSN_SET_STREAM ||
                        insn->set_opt == INSN_SET_SEC_STREAM ||
                        insn->set_opt == INSN_SET_BUTTON_PAGE) {
                        buf += _sprint_operands_hex(buf, cmd);
                    } else {
                        buf += _sprint_operands(buf, cmd);
                    }
                } else {
                    buf += sprintf(buf, "[unknown SETSYSTEM option in opcode 0x%08x] ", *(uint32_t*)insn);
                }
                break;
            default:
                buf += sprintf(buf, "[unknown SET subgroup in opcode 0x%08x] ", *(uint32_t*)insn);
                break;
            }
            break;

        default:
            buf += sprintf(buf, "[unknown group in opcode 0x%08x] ", *(uint32_t*)insn);
            break;
    }

    return buf - start;
}

