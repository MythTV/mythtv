/*
 * This file is part of libbluray
 * Copyright (C) 2010-2012  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#ifndef BD_OVERLAY_H_
#define BD_OVERLAY_H_

#include <stdint.h>

#define BD_OVERLAY_INTERFACE_VERSION 2

typedef enum {
    BD_OVERLAY_PG = 0,  /* Presentation Graphics plane */
    BD_OVERLAY_IG = 1,  /* Interactive Graphics plane (on top of PG plane) */
} bd_overlay_plane_e;

typedef enum {
    BD_OVERLAY_INIT,    /* init overlay plane. Size of full plane in x,y,w,h */
    BD_OVERLAY_CLEAR,   /* clear plane */
    BD_OVERLAY_DRAW,    /* draw bitmap (x,y,w,h,img,palette,crop) */
    BD_OVERLAY_WIPE,    /* clear area (x,y,w,h) */
    BD_OVERLAY_FLUSH,   /* all changes have been done, flush overlay to display at given pts */
    BD_OVERLAY_CLOSE,   /* close overlay */
} bd_overlay_cmd_e;

typedef struct bd_pg_palette_entry_s {
    uint8_t Y;
    uint8_t Cr;
    uint8_t Cb;
    uint8_t T;
} BD_PG_PALETTE_ENTRY;

typedef struct bd_pg_rle_elem_s {
    uint16_t len;
    uint16_t color;
} BD_PG_RLE_ELEM;

typedef struct bd_overlay_s {
    int64_t  pts;
    uint8_t  plane; /* bd_overlay_plane_e */
    uint8_t  cmd;   /* bd_overlay_cmd_e */

    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;

    const BD_PG_PALETTE_ENTRY * palette;
    const BD_PG_RLE_ELEM      * img;

    uint16_t crop_x;
    uint16_t crop_y;
    uint16_t crop_w;
    uint16_t crop_h;

} BD_OVERLAY;


#endif // BD_OVERLAY_H_
