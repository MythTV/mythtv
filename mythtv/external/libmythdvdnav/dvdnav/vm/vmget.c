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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dvdread/nav_types.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include "dvdnav/dvdnav.h"

#include "decoder.h"
#include "vm.h"
#include "getset.h"
#include "dvdnav_internal.h"

/* getting information */

int vm_get_current_menu(vm_t *vm, int *menuid) {
  pgcit_t* pgcit;
  int pgcn;
  pgcn = (vm->state).pgcN;
  pgcit = get_PGCIT(vm);
  if(pgcit==NULL) return 0;
  *menuid = pgcit->pgci_srp[pgcn - 1].entry_id & 0xf ;
  return 1;
}

int vm_get_current_title_part(vm_t *vm, int *title_result, int *part_result) {
  vts_ptt_srpt_t *vts_ptt_srpt;
  int title, part = 0, vts_ttn;
  int found;
  int16_t pgcN, pgN;

  vts_ptt_srpt = vm->vtsi->vts_ptt_srpt;
  pgcN = get_PGCN(vm);
  pgN = vm->state.pgN;

  found = 0;
  for (vts_ttn = 0; (vts_ttn < vts_ptt_srpt->nr_of_srpts) && !found; vts_ttn++) {
    for (part = 0; (part < vts_ptt_srpt->title[vts_ttn].nr_of_ptts) && !found; part++) {
      if (vts_ptt_srpt->title[vts_ttn].ptt[part].pgcn == pgcN) {
        if (vts_ptt_srpt->title[vts_ttn].ptt[part].pgn  == pgN) {
          found = 1;
          break;
        }
        if (part > 0 && vts_ptt_srpt->title[vts_ttn].ptt[part].pgn > pgN &&
            vts_ptt_srpt->title[vts_ttn].ptt[part - 1].pgn < pgN) {
          part--;
          found = 1;
          break;
        }
      }
    }
    if (found) break;
  }
  vts_ttn++;
  part++;

  if (!found) {
    fprintf(MSG_OUT, "libdvdnav: chapter NOT FOUND!\n");
    return 0;
  }

  title = get_TT(vm, vm->state.vtsN, vts_ttn);

#ifdef TRACE
  if (title) {
    fprintf(MSG_OUT, "libdvdnav: ************ this chapter FOUND!\n");
    fprintf(MSG_OUT, "libdvdnav: VTS_PTT_SRPT - Title %3i part %3i: PGC: %3i PG: %3i\n",
             title, part,
             vts_ptt_srpt->title[vts_ttn-1].ptt[part-1].pgcn ,
             vts_ptt_srpt->title[vts_ttn-1].ptt[part-1].pgn );
  }
#endif
  *title_result = title;
  *part_result = part;
  return 1;
}

/* Return the substream id for 'logical' audio stream audioN.
 * 0 <= audioN < 8
 */
int vm_get_audio_stream(vm_t *vm, int audioN) {
  int streamN = -1;

  if((vm->state).domain != DVD_DOMAIN_VTSTitle)
    audioN = 0;

  if(audioN < 8) {
    /* Is there any control info for this logical stream */
    if((vm->state).pgc->audio_control[audioN] & (1<<15)) {
      streamN = ((vm->state).pgc->audio_control[audioN] >> 8) & 0x07;
    }
  }

  if((vm->state).domain != DVD_DOMAIN_VTSTitle && streamN == -1)
    streamN = 0;

  /* FIXME: Should also check in vtsi/vmgi status what kind of stream
   * it is (ac3/lpcm/dts/sdds...) to find the right (sub)stream id */
  return streamN;
}

/* Return the substream id for 'logical' subpicture stream subpN and given mode.
 * 0 <= subpN < 32
 * mode == 0 - widescreen
 * mode == 1 - letterbox
 * mode == 2 - pan&scan
 */
int vm_get_subp_stream(vm_t *vm, int subpN, int mode) {
  int streamN = -1;
  int source_aspect = vm_get_video_aspect(vm);

  if((vm->state).domain != DVD_DOMAIN_VTSTitle)
    subpN = 0;

  if(subpN < 32) { /* a valid logical stream */
    /* Is this logical stream present */
    if((vm->state).pgc->subp_control[subpN] & (1<<31)) {
      if(source_aspect == 0) /* 4:3 */
        streamN = ((vm->state).pgc->subp_control[subpN] >> 24) & 0x1f;
      if(source_aspect == 3) /* 16:9 */
        switch (mode) {
        case 0:
          streamN = ((vm->state).pgc->subp_control[subpN] >> 16) & 0x1f;
          break;
        case 1:
          streamN = ((vm->state).pgc->subp_control[subpN] >> 8) & 0x1f;
          break;
        case 2:
          streamN = (vm->state).pgc->subp_control[subpN] & 0x1f;
        }
    }
  }

  if((vm->state).domain != DVD_DOMAIN_VTSTitle && streamN == -1)
    streamN = 0;

  /* FIXME: Should also check in vtsi/vmgi status what kind of stream it is. */
  return streamN;
}

int vm_get_audio_active_stream(vm_t *vm) {
  int audioN;
  int streamN;
  audioN = (vm->state).AST_REG ;
  streamN = vm_get_audio_stream(vm, audioN);

  /* If no such stream, then select the first one that exists. */
  if(streamN == -1) {
    for(audioN = 0; audioN < 8; audioN++) {
      if((vm->state).pgc->audio_control[audioN] & (1<<15)) {
        if ((streamN = vm_get_audio_stream(vm, audioN)) >= 0)
          break;
      }
    }
  }

  return streamN;
}

int vm_set_audio_active_stream(vm_t *vm, int audioN) {

    if (audioN >= 8)
      return -1;

    /* verify that stream exists */
    if(! ((vm->state).pgc->audio_control[audioN] & (1<<15)))
        return -1;

    (vm->state).AST_REG = audioN;
    return 0;
}

int vm_get_subp_active_stream(vm_t *vm, int mode) {
  int subpN;
  int streamN;
  subpN = (vm->state).SPST_REG & ~0x40;
  streamN = vm_get_subp_stream(vm, subpN, mode);

  /* If no such stream, then select the first one that exists. */
  if(streamN == -1) {
    for(subpN = 0; subpN < 32; subpN++) {
      if((vm->state).pgc->subp_control[subpN] & (1<<31)) {
        if ((streamN = vm_get_subp_stream(vm, subpN, mode)) >= 0)
          break;
      }
    }
  }

  if((vm->state).domain == DVD_DOMAIN_VTSTitle && !((vm->state).SPST_REG & 0x40))
    /* Bit 7 set means hide, and only let Forced display show */
    return (streamN | 0x80);
  else
    return streamN;
}

void vm_get_angle_info(vm_t *vm, int *current, int *num_avail) {
  *num_avail = 1;
  *current = 1;

  if((vm->state).domain == DVD_DOMAIN_VTSTitle) {
    title_info_t *title;
    /* TTN_REG does not allways point to the correct title.. */
    if((vm->state).TTN_REG > vm->vmgi->tt_srpt->nr_of_srpts)
      return;
    title = &vm->vmgi->tt_srpt->title[(vm->state).TTN_REG - 1];
    if(title->title_set_nr != (vm->state).vtsN ||
       title->vts_ttn != (vm->state).VTS_TTN_REG)
      return;
    *num_avail = title->nr_of_angles;
    *current = (vm->state).AGL_REG;
  }
}

#if 0
/* currently unused */
void vm_get_audio_info(vm_t *vm, int *current, int *num_avail) {
  switch ((vm->state).domain) {
  case DVD_DOMAIN_VTSTitle:
    *num_avail = vm->vtsi->vtsi_mat->nr_of_vts_audio_streams;
    *current = (vm->state).AST_REG;
    break;
  case DVD_DOMAIN_VTSMenu:
    *num_avail = vm->vtsi->vtsi_mat->nr_of_vtsm_audio_streams; /*  1 */
    *current = 1;
    break;
  case DVD_DOMAIN_VMGM:
  case DVD_DOMAIN_FirstPlay:
    *num_avail = vm->vmgi->vmgi_mat->nr_of_vmgm_audio_streams; /*  1 */
    *current = 1;
    break;
  }
}

/* currently unused */
void vm_get_subp_info(vm_t *vm, int *current, int *num_avail) {
  switch ((vm->state).domain) {
  case DVD_DOMAIN_VTSTitle:
    *num_avail = vm->vtsi->vtsi_mat->nr_of_vts_subp_streams;
    *current = (vm->state).SPST_REG;
    break;
  case DVD_DOMAIN_VTSMenu:
    *num_avail = vm->vtsi->vtsi_mat->nr_of_vtsm_subp_streams; /*  1 */
    *current = 0x41;
    break;
  case DVD_DOMAIN_VMGM:
  case DVD_DOMAIN_FirstPlay:
    *num_avail = vm->vmgi->vmgi_mat->nr_of_vmgm_subp_streams; /*  1 */
    *current = 0x41;
    break;
  }
}
#endif

void vm_get_video_res(vm_t *vm, int *width, int *height) {
  video_attr_t attr = vm_get_video_attr(vm);

  if(attr.video_format != 0)
    *height = 576;
  else
    *height = 480;
  switch(attr.picture_size) {
  case 0:
    *width = 720;
    break;
  case 1:
    *width = 704;
    break;
  case 2:
    *width = 352;
    break;
  case 3:
    *width = 352;
    *height /= 2;
    break;
  }
}

int vm_get_video_aspect(vm_t *vm) {
  int aspect = vm_get_video_attr(vm).display_aspect_ratio;

  if(aspect != 0 && aspect != 3) {
    fprintf(MSG_OUT, "libdvdnav: display aspect ratio is unexpected: %d!\n", aspect);
    return -1;
  }

  (vm->state).registers.SPRM[14] &= ~(0x3 << 10);
  (vm->state).registers.SPRM[14] |= aspect << 10;

  return aspect;
}

int vm_get_video_scale_permission(vm_t *vm) {
  return vm_get_video_attr(vm).permitted_df;
}

int vm_get_video_format(vm_t *vm) {
  return vm_get_video_attr(vm).video_format;
}

video_attr_t vm_get_video_attr(vm_t *vm) {
  switch ((vm->state).domain) {
  case DVD_DOMAIN_VTSTitle:
    return vm->vtsi->vtsi_mat->vts_video_attr;
  case DVD_DOMAIN_VTSMenu:
    return vm->vtsi->vtsi_mat->vtsm_video_attr;
  case DVD_DOMAIN_VMGM:
  case DVD_DOMAIN_FirstPlay:
    return vm->vmgi->vmgi_mat->vmgm_video_attr;
  default:
    assert(0);
  }
  video_attr_t ret = {};
  return ret;
}

audio_attr_t vm_get_audio_attr(vm_t *vm, int streamN) {
  switch ((vm->state).domain) {
  case DVD_DOMAIN_VTSTitle:
    return vm->vtsi->vtsi_mat->vts_audio_attr[streamN];
  case DVD_DOMAIN_VTSMenu:
    return vm->vtsi->vtsi_mat->vtsm_audio_attr;
  case DVD_DOMAIN_VMGM:
  case DVD_DOMAIN_FirstPlay:
    return vm->vmgi->vmgi_mat->vmgm_audio_attr;
  default:
    assert(0);
  }

  audio_attr_t ret = {};
  return ret;
}

subp_attr_t vm_get_subp_attr(vm_t *vm, int streamN) {
  switch ((vm->state).domain) {
  case DVD_DOMAIN_VTSTitle:
    return vm->vtsi->vtsi_mat->vts_subp_attr[streamN];
  case DVD_DOMAIN_VTSMenu:
    return vm->vtsi->vtsi_mat->vtsm_subp_attr;
  case DVD_DOMAIN_VMGM:
  case DVD_DOMAIN_FirstPlay:
    return vm->vmgi->vmgi_mat->vmgm_subp_attr;
  default:
    assert(0);
  }

  subp_attr_t ret = {};
  return ret;
}
