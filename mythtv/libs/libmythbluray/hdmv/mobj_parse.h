/*
 * This file is part of libbluray
 * Copyright (C) 2010  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_MOBJ_PARSE_H_)
#define _MOBJ_PARSE_H_

#include <util/attributes.h>

#include <stdint.h>

typedef struct {
  uint8_t sub_grp    : 3;  /* command sub-group */
  uint8_t op_cnt     : 3;  /* operand count */
  uint8_t grp        : 2;  /* command group */

  uint8_t branch_opt : 4;  /* branch option */
  uint8_t reserved1  : 2;
  uint8_t imm_op2    : 1;  /* I-flag for operand 2 */
  uint8_t imm_op1    : 1;  /* I-flag for operand 1 */

  uint8_t cmp_opt    : 4;  /* compare option */
  uint8_t reserved2  : 4;

  uint8_t set_opt    : 5;  /* set option */
  uint8_t reserved3  : 3;
} HDMV_INSN;

typedef struct bd_mobj_cmd_s {
    HDMV_INSN insn;
    uint32_t  dst;
    uint32_t  src;
} MOBJ_CMD;

typedef struct {
    uint8_t     resume_intention_flag : 1;
    uint8_t     menu_call_mask        : 1;
    uint8_t     title_search_mask     : 1;

    uint16_t    num_cmds;
    MOBJ_CMD   *cmds;
} MOBJ_OBJECT;

typedef struct {
    uint16_t    num_objects;
    MOBJ_OBJECT *objects;
} MOBJ_OBJECTS;


BD_PRIVATE MOBJ_OBJECTS* mobj_parse(const char *path) BD_ATTR_MALLOC; /* parse MovieObject.bdmv */
BD_PRIVATE void          mobj_parse_cmd(uint8_t *buf, MOBJ_CMD *cmd);
BD_PRIVATE void          mobj_free(MOBJ_OBJECTS **index);

BD_PRIVATE int mobj_sprint_cmd(char *buf, MOBJ_CMD *cmd); /* print MOBJ_CMD to string. buf is expected to be 256 bytes. */

#endif // _MOBJ_PARSE_H_
