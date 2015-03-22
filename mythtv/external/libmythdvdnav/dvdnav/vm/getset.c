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
#include <inttypes.h>

#include <dvdread/nav_types.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include "dvdnav/dvdnav.h"

#include "decoder.h"
#include "vm.h"
#include "getset.h"
#include "dvdnav_internal.h"

#include "getset.h"
/* Set functions */

int set_TT(vm_t *vm, int tt) {
  return set_PTT(vm, tt, 1);
}

int set_PTT(vm_t *vm, int tt, int ptt) {
  assert(tt <= vm->vmgi->tt_srpt->nr_of_srpts);
  return set_VTS_PTT(vm, vm->vmgi->tt_srpt->title[tt - 1].title_set_nr,
                     vm->vmgi->tt_srpt->title[tt - 1].vts_ttn, ptt);
}

int set_VTS_TT(vm_t *vm, int vtsN, int vts_ttn) {
  return set_VTS_PTT(vm, vtsN, vts_ttn, 1);
}

int set_VTS_PTT(vm_t *vm, int vtsN, int vts_ttn, int part) {
  int pgcN, pgN, res;

  (vm->state).domain = DVD_DOMAIN_VTSTitle;

  if (vtsN != (vm->state).vtsN)
    if (!ifoOpenNewVTSI(vm, vm->dvd, vtsN))  /* Also sets (vm->state).vtsN */
      return 0;

  if ((vts_ttn < 1) || (vts_ttn > vm->vtsi->vts_ptt_srpt->nr_of_srpts) ||
      (part < 1) || (part > vm->vtsi->vts_ptt_srpt->title[vts_ttn - 1].nr_of_ptts) ) {
    return 0;
  }

  pgcN = vm->vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[part - 1].pgcn;
  pgN = vm->vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[part - 1].pgn;

  (vm->state).TT_PGCN_REG = pgcN;
  (vm->state).PTTN_REG    = part;
  (vm->state).TTN_REG     = get_TT(vm, vtsN, vts_ttn);
  if( (vm->state.TTN_REG) == 0 )
    return 0;

  (vm->state).VTS_TTN_REG = vts_ttn;
  (vm->state).vtsN        = vtsN;  /* Not sure about this one. We can get to it easily from TTN_REG */
  /* Any other registers? */

  res = set_PGCN(vm, pgcN);   /* This clobber's state.pgN (sets it to 1), but we don't want clobbering here. */
  (vm->state).pgN = pgN;
  return res;
}

int set_PROG(vm_t *vm, int tt, int pgcn, int pgn) {
  assert(tt <= vm->vmgi->tt_srpt->nr_of_srpts);
  return set_VTS_PROG(vm, vm->vmgi->tt_srpt->title[tt - 1].title_set_nr,
                     vm->vmgi->tt_srpt->title[tt - 1].vts_ttn, pgcn, pgn);
}

int set_VTS_PROG(vm_t *vm, int vtsN, int vts_ttn, int pgcn, int pgn) {
  int pgcN, pgN, res, title, part = 0;

  (vm->state).domain = DVD_DOMAIN_VTSTitle;

  if (vtsN != (vm->state).vtsN)
    if (!ifoOpenNewVTSI(vm, vm->dvd, vtsN))  /* Also sets (vm->state).vtsN */
      return 0;

  if ((vts_ttn < 1) || (vts_ttn > vm->vtsi->vts_ptt_srpt->nr_of_srpts)) {
    return 0;
  }

  pgcN = pgcn;
  pgN = pgn;

  (vm->state).TT_PGCN_REG = pgcN;
  (vm->state).TTN_REG     = get_TT(vm, vtsN, vts_ttn);
  assert( (vm->state.TTN_REG) != 0 );
  (vm->state).VTS_TTN_REG = vts_ttn;
  (vm->state).vtsN        = vtsN;  /* Not sure about this one. We can get to it easily from TTN_REG */
  /* Any other registers? */

  res = set_PGCN(vm, pgcN);   /* This clobber's state.pgN (sets it to 1), but we don't want clobbering here. */
  (vm->state).pgN = pgN;
  vm_get_current_title_part(vm, &title, &part);
  (vm->state).PTTN_REG    = part;
  return res;
}

int set_FP_PGC(vm_t *vm) {
  if (!vm || !vm->vmgi)
      return 1;
  (vm->state).domain = DVD_DOMAIN_FirstPlay;
  if (!vm->vmgi->first_play_pgc) {
    return set_PGCN(vm, 1);
  }
  (vm->state).pgc = vm->vmgi->first_play_pgc;
  (vm->state).pgcN = vm->vmgi->vmgi_mat->first_play_pgc;
  return 1;
}


int set_MENU(vm_t *vm, int menu) {
  assert((vm->state).domain == DVD_DOMAIN_VMGM || (vm->state).domain == DVD_DOMAIN_VTSMenu);
  return set_PGCN(vm, get_ID(vm, menu));
}

int set_PGCN(vm_t *vm, int pgcN) {
  pgcit_t *pgcit;

  pgcit = get_PGCIT(vm);
  if (pgcit == NULL)
    return 0;

  if(pgcN < 1 || pgcN > pgcit->nr_of_pgci_srp) {
#ifdef TRACE
    fprintf(MSG_OUT, "libdvdnav:  ** No such pgcN = %d\n", pgcN);
#endif
    return 0;
  }

  (vm->state).pgc = pgcit->pgci_srp[pgcN - 1].pgc;
  (vm->state).pgcN = pgcN;
  (vm->state).pgN = 1;

  if((vm->state).domain == DVD_DOMAIN_VTSTitle)
    (vm->state).TT_PGCN_REG = pgcN;

  return 1;
}

/* Figure out the correct pgN from the cell and update (vm->state). */
int set_PGN(vm_t *vm) {
  int new_pgN = 0;
  int dummy, part = 0;

  while(new_pgN < (vm->state).pgc->nr_of_programs
        && (vm->state).cellN >= (vm->state).pgc->program_map[new_pgN])
    new_pgN++;

  if(new_pgN == (vm->state).pgc->nr_of_programs) /* We are at the last program */
    if((vm->state).cellN > (vm->state).pgc->nr_of_cells)
      return 0; /* We are past the last cell */

  (vm->state).pgN = new_pgN;

  if((vm->state).domain == DVD_DOMAIN_VTSTitle) {
    if((vm->state).TTN_REG > vm->vmgi->tt_srpt->nr_of_srpts)
      return 0; /* ?? */

    vm_get_current_title_part(vm, &dummy, &part);
    (vm->state).PTTN_REG = part;
  }
  return 1;
}

/* Must be called before domain is changed (set_PGCN()) */
void set_RSMinfo(vm_t *vm, int cellN, int blockN) {
  int i;

  if(cellN) {
    (vm->state).rsm_cellN = cellN;
    (vm->state).rsm_blockN = blockN;
  } else {
    (vm->state).rsm_cellN = (vm->state).cellN;
    (vm->state).rsm_blockN = blockN;
  }
  (vm->state).rsm_vtsN = (vm->state).vtsN;
  (vm->state).rsm_pgcN = get_PGCN(vm);

  /* assert((vm->state).rsm_pgcN == (vm->state).TT_PGCN_REG);  for DVD_DOMAIN_VTSTitle */

  for(i = 0; i < 5; i++) {
    (vm->state).rsm_regs[i] = (vm->state).registers.SPRM[4 + i];
  }
}

/* Get functions */

/* Searches the TT tables, to find the current TT.
 * returns the current TT.
 * returns 0 if not found.
 */
int get_TT(vm_t *vm, int vtsN, int vts_ttn) {
  int i;
  int tt=0;

  for(i = 1; i <= vm->vmgi->tt_srpt->nr_of_srpts; i++) {
    if( vm->vmgi->tt_srpt->title[i - 1].title_set_nr == vtsN &&
        vm->vmgi->tt_srpt->title[i - 1].vts_ttn == vts_ttn) {
      tt=i;
      break;
    }
  }
  return tt;
}

/* Search for entry_id match of the PGC Category in the current VTS PGCIT table.
 * Return pgcN based on entry_id match.
 */
int get_ID(vm_t *vm, int id) {
  int pgcN, i;
  pgcit_t *pgcit;

  /* Relies on state to get the correct pgcit. */
  pgcit = get_PGCIT(vm);
  if(pgcit == NULL) {
    fprintf(MSG_OUT, "libdvdnav: PGCIT null!\n");
    return 0;
  }
#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: ** Searching for menu (0x%x) entry PGC\n", id);
#endif

  /* Force high bit set. */
  id |=0x80;

  /* Get menu/title */
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    if( (pgcit->pgci_srp[i].entry_id) == id) {
      pgcN = i + 1;
#ifdef TRACE
      fprintf(MSG_OUT, "libdvdnav: Found menu.\n");
#endif
      return pgcN;
    }
  }
#ifdef TRACE
  fprintf(MSG_OUT, "libdvdnav: ** No such id/menu (0x%02x) entry PGC\n", id & 0x7f);
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    if ( (pgcit->pgci_srp[i].entry_id & 0x80) == 0x80) {
      fprintf(MSG_OUT, "libdvdnav: Available menus: 0x%x\n",
                     pgcit->pgci_srp[i].entry_id & 0x7f);
    }
  }
#endif
  return 0; /*  error */
}

/* FIXME: we have a pgcN member in the vm's state now, so this should be obsolete */
int get_PGCN(vm_t *vm) {
  pgcit_t *pgcit;
  int pgcN = 1;

  pgcit = get_PGCIT(vm);

  if (pgcit) {
    while(pgcN <= pgcit->nr_of_pgci_srp) {
      if(pgcit->pgci_srp[pgcN - 1].pgc == (vm->state).pgc) {
        assert((vm->state).pgcN == pgcN);
        return pgcN;
      }
      pgcN++;
    }
  }
  fprintf(MSG_OUT, "libdvdnav: get_PGCN failed. Was trying to find pgcN in domain %d\n",
         (vm->state).domain);
  return 0; /*  error */
}

pgcit_t* get_MENU_PGCIT(vm_t *vm, ifo_handle_t *h, uint16_t lang) {
  int i;

  if(h == NULL || h->pgci_ut == NULL) {
    fprintf(MSG_OUT, "libdvdnav: *** pgci_ut handle is NULL ***\n");
    return NULL; /*  error? */
  }

  i = 0;
  while(i < h->pgci_ut->nr_of_lus
        && h->pgci_ut->lu[i].lang_code != lang)
    i++;
  if(i == h->pgci_ut->nr_of_lus) {
    fprintf(MSG_OUT, "libdvdnav: Language '%c%c' not found, using '%c%c' instead\n",
            (char)(lang >> 8), (char)(lang & 0xff),
             (char)(h->pgci_ut->lu[0].lang_code >> 8),
            (char)(h->pgci_ut->lu[0].lang_code & 0xff));
    fprintf(MSG_OUT, "libdvdnav: Menu Languages available: ");
    for(i = 0; i < h->pgci_ut->nr_of_lus; i++) {
      fprintf(MSG_OUT, "%c%c ",
             (char)(h->pgci_ut->lu[i].lang_code >> 8),
            (char)(h->pgci_ut->lu[i].lang_code & 0xff));
    }
    fprintf(MSG_OUT, "\n");
    i = 0; /*  error? */
  }

  return h->pgci_ut->lu[i].pgcit;
}

/* Uses state to decide what to return */
pgcit_t* get_PGCIT(vm_t *vm) {
  pgcit_t *pgcit = NULL;

  switch ((vm->state).domain) {
  case DVD_DOMAIN_VTSTitle:
    if(!vm->vtsi) return NULL;
    pgcit = vm->vtsi->vts_pgcit;
    break;
  case DVD_DOMAIN_VTSMenu:
    if(!vm->vtsi) return NULL;
    pgcit = get_MENU_PGCIT(vm, vm->vtsi, (vm->state).registers.SPRM[0]);
    break;
  case DVD_DOMAIN_VMGM:
  case DVD_DOMAIN_FirstPlay:
    pgcit = get_MENU_PGCIT(vm, vm->vmgi, (vm->state).registers.SPRM[0]);
    break;
  default:
    abort();
  }

  return pgcit;
}

