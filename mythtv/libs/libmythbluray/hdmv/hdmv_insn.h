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

#if !defined(_HDMV_INSN_H_)
#define _HDMV_INSN_H_

/*
 * instruction groups
 */

typedef enum {
    INSN_GROUP_BRANCH = 0,
    INSN_GROUP_CMP    = 1,
    INSN_GROUP_SET    = 2,
} hdmv_insn_grp;

/*
 * BRANCH group
 */

/* BRANCH sub-groups */
typedef enum {
    BRANCH_GOTO   = 0x00,
    BRANCH_JUMP   = 0x01,
    BRANCH_PLAY   = 0x02,
} hdmv_insn_grp_branch;

/* GOTO sub-group */
typedef enum {
    INSN_NOP          = 0x00,
    INSN_GOTO         = 0x01,
    INSN_BREAK        = 0x02,
} hdmv_insn_goto;

/* JUMP sub-group */
typedef enum {
    INSN_JUMP_OBJECT  = 0x00,
    INSN_JUMP_TITLE   = 0x01,
    INSN_CALL_OBJECT  = 0x02,
    INSN_CALL_TITLE   = 0x03,
    INSN_RESUME       = 0x04,
} hdmv_insn_jump;

/* PLAY sub-group */
typedef enum {
    INSN_PLAY_PL      = 0x00,
    INSN_PLAY_PL_PI   = 0x01,
    INSN_PLAY_PL_PM   = 0x02,
    INSN_TERMINATE_PL = 0x03,
    INSN_LINK_PI      = 0x04,
    INSN_LINK_MK      = 0x05,
} hdmv_insn_play;

/*
 * COMPARE group
 */

typedef enum {
    INSN_BC = 0x01,
    INSN_EQ = 0x02,
    INSN_NE = 0x03,
    INSN_GE = 0x04,
    INSN_GT = 0x05,
    INSN_LE = 0x06,
    INSN_LT = 0x07,
} hdmv_insn_cmp;

/*
 * SET group
 */

/* SET sub-groups */
typedef enum {
    SET_SET       = 0x00,
    SET_SETSYSTEM = 0x01,
} hdmv_insn_grp_set;

/* SET sub-group */
typedef enum {
    INSN_MOVE   = 0x01,
    INSN_SWAP   = 0x02,
    INSN_ADD    = 0x03,
    INSN_SUB    = 0x04,
    INSN_MUL    = 0x05,
    INSN_DIV    = 0x06,
    INSN_MOD    = 0x07,
    INSN_RND    = 0x08,
    INSN_AND    = 0x09,
    INSN_OR     = 0x0a,
    INSN_XOR    = 0x0b,
    INSN_BITSET = 0x0c,
    INSN_BITCLR = 0x0d,
    INSN_SHL    = 0x0e,
    INSN_SHR    = 0x0f,
} hdmv_insn_set;

/* SETSYSTEM sub-group */
typedef enum {
    INSN_SET_STREAM      = 0x01,
    INSN_SET_NV_TIMER    = 0x02,
    INSN_SET_BUTTON_PAGE = 0x03,
    INSN_ENABLE_BUTTON   = 0x04,
    INSN_DISABLE_BUTTON  = 0x05,
    INSN_SET_SEC_STREAM  = 0x06,
    INSN_POPUP_OFF       = 0x07,
    INSN_STILL_ON        = 0x08,
    INSN_STILL_OFF       = 0x09,
} hdmv_insn_setsystem;

#endif // _HDMV_INSN_H_
