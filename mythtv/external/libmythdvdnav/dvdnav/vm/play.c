/*
 * Copyright (C) 2000, 2001 Håkan Hjort
 * Copyright (C) 2001 Rich Wareham <richwareham@users.sourceforge.net>
 *               2002-2004 the dvdnav project
 *
 * This file is part of libdvdnav, a DVD navigation library. It is modified
 * from a file originally part of the Ogle DVD player.
 *
 * libdvdnav is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdnav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdnav; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <dvdread/nav_types.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include "dvdnav/dvdnav.h"

#include "decoder.h"
#include "vm.h"
#include "play.h"
#include "vm/getset.h"

#include "dvdnav_internal.h"

/* Playback control */

link_t play_PGC(vm_t *vm) {
  link_t link_values;

#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: play_PGC:");
  if((vm->state).domain != DVD_DOMAIN_FirstPlay) {
    fprintf(MSG_OUT, " (vm->state).pgcN (%i)\n", get_PGCN(vm));
  } else {
    fprintf(MSG_OUT, " first_play_pgc\n");
  }
#endif

  /* This must be set before the pre-commands are executed because they
   * might contain a CallSS that will save resume state */

  /* FIXME: This may be only a temporary fix for something... */
  (vm->state).pgN = 1;
  (vm->state).cellN = 0;
  (vm->state).blockN = 0;

  /* eval -> updates the state and returns either
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just play video i.e first PG
       (This is what happens if you fall of the end of the pre_cmds)
     - or an error (are there more cases?) */
  if((vm->state).pgc->command_tbl && (vm->state).pgc->command_tbl->nr_of_pre) {
    if(vmEval_CMD((vm->state).pgc->command_tbl->pre_cmds,
                  (vm->state).pgc->command_tbl->nr_of_pre,
                  &(vm->state).registers, &link_values)) {
      /*  link_values contains the 'jump' return value */
      return link_values;
    } else {
#ifdef TRACE
      fprintf(MSG_OUT, "libdvdnav: PGC pre commands didn't do a Jump, Link or Call\n");
#endif
    }
  }
  return play_PG(vm);
}

link_t play_PGC_PG(vm_t *vm, int pgN) {
  link_t link_values;

#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: play_PGC_PG:");
  if((vm->state).domain != DVD_DOMAIN_FirstPlay) {
    fprintf(MSG_OUT, " (vm->state).pgcN (%i)\n", get_PGCN(vm));
  } else {
    fprintf(MSG_OUT, " first_play_pgc\n");
  }
#endif

  /*  This must be set before the pre-commands are executed because they
   *  might contain a CallSS that will save resume state */

  /* FIXME: This may be only a temporary fix for something... */
  (vm->state).pgN = pgN;
  (vm->state).cellN = 0;
  (vm->state).blockN = 0;

  /* eval -> updates the state and returns either
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just play video i.e first PG
       (This is what happens if you fall of the end of the pre_cmds)
     - or an error (are there more cases?) */
  if((vm->state).pgc->command_tbl && (vm->state).pgc->command_tbl->nr_of_pre) {
    if(vmEval_CMD((vm->state).pgc->command_tbl->pre_cmds,
                  (vm->state).pgc->command_tbl->nr_of_pre,
                  &(vm->state).registers, &link_values)) {
      /*  link_values contains the 'jump' return value */
      return link_values;
    } else {
#ifdef TRACE
      fprintf(MSG_OUT, "libdvdnav: PGC pre commands didn't do a Jump, Link or Call\n");
#endif
    }
  }
  return play_PG(vm);
}

link_t play_PGC_post(vm_t *vm) {
  link_t link_values = { LinkNoLink, 0, 0, 0 };

#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: play_PGC_post:\n");
#endif

  /* eval -> updates the state and returns either
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just go to next PGC
       (This is what happens if you fall of the end of the post_cmds)
     - or an error (are there more cases?) */
  if((vm->state).pgc->command_tbl && (vm->state).pgc->command_tbl->nr_of_post &&
     vmEval_CMD((vm->state).pgc->command_tbl->post_cmds,
                (vm->state).pgc->command_tbl->nr_of_post,
                &(vm->state).registers, &link_values)) {
    return link_values;
  }

#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: ** Fell of the end of the pgc, continuing in NextPGC\n");
#endif
  /* Should end up in the STOP_DOMAIN if next_pgc is 0. */
  if(!set_PGCN(vm, (vm->state).pgc->next_pgc_nr)) {
    link_values.command = Exit;
    return link_values;
  }
  return play_PGC(vm);
}

link_t play_PG(vm_t *vm) {
#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: play_PG: (vm->state).pgN (%i)\n", (vm->state).pgN);
#endif

  assert((vm->state).pgN > 0);
  if((vm->state).pgN > (vm->state).pgc->nr_of_programs) {
#ifdef TRACE
    fprintf(MSG_OUT, "libdvdnav: play_PG: (vm->state).pgN (%i) > pgc->nr_of_programs (%i)\n",
            (vm->state).pgN, (vm->state).pgc->nr_of_programs );
#endif
    assert((vm->state).pgN == (vm->state).pgc->nr_of_programs + 1);
    return play_PGC_post(vm);
  }

  (vm->state).cellN = (vm->state).pgc->program_map[(vm->state).pgN - 1];

  return play_Cell(vm);
}

link_t play_Cell(vm_t *vm) {
  static const link_t play_this = {PlayThis, /* Block in Cell */ 0, 0, 0};

#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: play_Cell: (vm->state).cellN (%i)\n", (vm->state).cellN);
#endif

  assert((vm->state).cellN > 0);
  if((vm->state).cellN > (vm->state).pgc->nr_of_cells) {
#ifdef TRACE
    fprintf(MSG_OUT, "libdvdnav: (vm->state).cellN (%i) > pgc->nr_of_cells (%i)\n",
            (vm->state).cellN, (vm->state).pgc->nr_of_cells );
#endif
    assert((vm->state).cellN == (vm->state).pgc->nr_of_cells + 1);
    return play_PGC_post(vm);
  }

  /* Multi angle/Interleaved */
  switch((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_mode) {
  case 0: /*  Normal */
    /* MythTV assert((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_type == 0); */
    break;
  case 1: /*  The first cell in the block */
    switch((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_type) {
    case 0: /*  Not part of a block */
      assert(0);
      break;
    case 1: /*  Angle block */
      /* Loop and check each cell instead? So we don't get outside the block? */
      (vm->state).cellN += (vm->state).AGL_REG - 1;
#ifdef DVDNAV_STRICT
      assert((vm->state).cellN <= (vm->state).pgc->nr_of_cells);
      assert((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_mode != 0);
      assert((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_type == 1);
#else
      if (!((vm->state).cellN <= (vm->state).pgc->nr_of_cells) ||
          !((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_mode != 0) ||
          !((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_type == 1)) {
        fprintf(MSG_OUT, "libdvdnav: Invalid angle block\n");
        (vm->state).cellN -= (vm->state).AGL_REG - 1;
      }
#endif
      break;
    case 2: /*  ?? */
    case 3: /*  ?? */
    default:
      fprintf(MSG_OUT, "libdvdnav: Invalid? Cell block_mode (%d), block_type (%d)\n",
              (vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_mode,
              (vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_type);
      assert(0);
    }
    break;
  case 2: /*  Cell in the block */
  case 3: /*  Last cell in the block */
  /* These might perhaps happen for RSM or LinkC commands? */
  default:
    fprintf(MSG_OUT, "libdvdnav: Cell is in block but did not enter at first cell!\n");
  }

  /* Updates (vm->state).pgN and PTTN_REG */
  if(!set_PGN(vm)) {
    /* Should not happen */
    assert(0);
    return play_PGC_post(vm);
  }
  (vm->state).cell_restart++;
  (vm->state).blockN = 0;
#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: Cell should restart here\n");
#endif
  return play_this;
}

link_t play_Cell_post(vm_t *vm) {
  cell_playback_t *cell;

#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: play_Cell_post: (vm->state).cellN (%i)\n", (vm->state).cellN);
#endif

  cell = &(vm->state).pgc->cell_playback[(vm->state).cellN - 1];

  /* Still time is already taken care of before we get called. */

  /* Deal with a Cell command, if any */
  if(cell->cell_cmd_nr != 0) {
    link_t link_values;

    if ((vm->state).pgc->command_tbl != NULL &&
        (vm->state).pgc->command_tbl->nr_of_cell >= cell->cell_cmd_nr) {
#ifdef TRACE
      fprintf(MSG_OUT, "libdvdnav: Cell command present, executing\n");
#endif
      if(vmEval_CMD(&(vm->state).pgc->command_tbl->cell_cmds[cell->cell_cmd_nr - 1], 1,
                    &(vm->state).registers, &link_values)) {
        return link_values;
      } else {
#ifdef TRACE
        fprintf(MSG_OUT, "libdvdnav: Cell command didn't do a Jump, Link or Call\n");
#endif
      }
    } else {
#ifdef TRACE
      fprintf(MSG_OUT, "libdvdnav: Invalid Cell command\n");
#endif
    }
  }

  /* Where to continue after playing the cell... */
  /* Multi angle/Interleaved */
  switch((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_mode) {
  case 0: /*  Normal */
    /* MythTV assert((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_type == 0); */
    (vm->state).cellN++;
    break;
  case 1: /*  The first cell in the block */
  case 2: /*  A cell in the block */
  case 3: /*  The last cell in the block */
  default:
    switch((vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_type) {
    case 0: /*  Not part of a block */
      assert(0);
      break;
    case 1: /*  Angle block */
      /* Skip the 'other' angles */
      (vm->state).cellN++;
      while((vm->state).cellN <= (vm->state).pgc->nr_of_cells &&
            (vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_mode >= 2) {
        (vm->state).cellN++;
      }
      break;
    case 2: /*  ?? */
    case 3: /*  ?? */
    default:
      fprintf(MSG_OUT, "libdvdnav: Invalid? Cell block_mode (%d), block_type (%d)\n",
              (vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_mode,
              (vm->state).pgc->cell_playback[(vm->state).cellN - 1].block_type);
      assert(0);
    }
    break;
  }

  /* Figure out the correct pgN for the new cell */
  if(!set_PGN(vm)) {
#ifdef TRACE
    fprintf(MSG_OUT, "libdvdnav: last cell in this PGC\n");
#endif
    return play_PGC_post(vm);
  }
  return play_Cell(vm);
}
