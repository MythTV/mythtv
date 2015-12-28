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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BD_OVERLAY_INTERFACE_VERSION 2

typedef enum {
    BD_OVERLAY_PG = 0,  /* Presentation Graphics plane */
    BD_OVERLAY_IG = 1,  /* Interactive Graphics plane (on top of PG plane) */
} bd_overlay_plane_e;

typedef enum {
    /* following events are executed immediately */
    BD_OVERLAY_INIT = 0,    /* init overlay plane. Size and position of plane in x,y,w,h */
    BD_OVERLAY_CLOSE = 5,   /* close overlay plane */

    /* following events can be processed immediately, but changes
     * should not be flushed to display before next FLUSH event
     */
    BD_OVERLAY_CLEAR = 1,   /* clear plane */
    BD_OVERLAY_DRAW = 2,    /* draw bitmap (x,y,w,h,img,palette,crop) */
    BD_OVERLAY_WIPE = 3,    /* clear area (x,y,w,h) */
    BD_OVERLAY_HIDE = 6,    /* overlay is empty and can be hidden */

    BD_OVERLAY_FLUSH = 4,   /* all changes have been done, flush overlay to display at given pts */

} bd_overlay_cmd_e;

/*
 * Compressed YUV overlays
 */

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

    uint16_t crop_x; /* deprecated: cropping is executed by libbluray */
    uint16_t crop_y; /* deprecated: cropping is executed by libbluray */
    uint16_t crop_w; /* deprecated: cropping is executed by libbluray */
    uint16_t crop_h; /* deprecated: cropping is executed by libbluray */

    uint8_t palette_update_flag; /* only palette was changed */
} BD_OVERLAY;

/*
  RLE images are reference-counted. If application caches rle data for later use,
  it needs to use bd_refcnt_inc() and bd_refcnt_dec().
*/

void bd_refcnt_inc(const void *);
void bd_refcnt_dec(const void *);

#if 0
BD_OVERLAY *bd_overlay_copy(const BD_OVERLAY *src)
{
    BD_OVERLAY *ov = malloc(sizeof(*ov));
    memcpy(ov, src, sizeof(*ov));
    if (ov->palette) {
        ov->palette = malloc(256 * sizeof(BD_PG_PALETTE_ENTRY));
        memcpy((void*)ov->palette, src->palette, 256 * sizeof(BD_PG_PALETTE_ENTRY));
    }
    if (ov->img) {
        bd_refcnt_inc(ov->img);
    }
    return ov;
}

void bd_overlay_free(BD_OVERLAY **pov)
{
    if (pov && *pov) {
        BD_OVERLAY *ov = *pov;
        void *p = (void*)ov->palette;
        bd_refcnt_dec(ov->img);
        X_FREE(p);
        ov->palette = NULL;
        X_FREE(*pov);
    }
}
#endif

/*
 * ARGB overlays
 */

typedef enum {
    /* following events are executed immediately */
    BD_ARGB_OVERLAY_INIT = 0,    /* init overlay plane. Size and position of plane in x,y,w,h */
    BD_ARGB_OVERLAY_CLOSE = 5,   /* close overlay */

    /* following events can be processed immediately, but changes
     * should not be flushed to display before next FLUSH event
     */
    BD_ARGB_OVERLAY_DRAW = 2,    /* draw image */
    BD_ARGB_OVERLAY_FLUSH = 4,   /* all changes have been done, flush overlay to display at given pts */
} bd_argb_overlay_cmd_e;

typedef struct bd_argb_overlay_s {
    int64_t  pts;
    uint8_t  plane; /* bd_overlay_plane_e */
    uint8_t  cmd;   /* bd_argb_overlay_cmd_e */

    /* following fileds are used only when not using application-allocated
     * frame buffer
     */

    /* destination clip on the overlay plane
     */
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;

    const uint32_t * argb; /* 'h' lines, line length 'stride' pixels */
    uint16_t stride;       /* buffer stride */

} BD_ARGB_OVERLAY;

/*
 * Application-allocated frame buffer for ARGB overlays
 *
 * When using application-allocated frame buffer DRAW events are
 * executed by libbluray.
 * Application needs to handle only OPEN/FLUSH/CLOSE events.
 *
 * DRAW events can still be used for optimizations.
 */
typedef struct bd_argb_buffer_s {
    /* optional lock / unlock functions
     *  - Set by application
     *  - Called when buffer is accessed or modified
     */
    void (*lock)  (struct bd_argb_buffer_s *);
    void (*unlock)(struct bd_argb_buffer_s *);

    /* ARGB frame buffers
     * - Allocated by application (BD_ARGB_OVERLAY_INIT).
     * - Buffer can be freed after BD_ARGB_OVERLAY_CLOSE.
     * - buffer can be replaced in overlay callback or lock().
     */

    uint32_t *buf[2]; /* [0] - PG plane, [1] - IG plane */

    /* size of buffers
     * - Set by application
     * - If the buffer size is smaller than the size requested in BD_ARGB_OVERLAY_INIT,
     *   the buffer points only to the dirty area.
     */
    int width;
    int height;

    /* dirty area of frame buffers
     * - Updated by library before lock() call.
     * - Reset after each BD_ARGB_OVERLAY_FLUSH.
     */
    struct {
        uint16_t x0, y0, x1, y1;
    } dirty[2]; /* [0] - PG plane, [1] - IG plane */

} BD_ARGB_BUFFER;

#ifdef __cplusplus
}
#endif

#endif // BD_OVERLAY_H_
