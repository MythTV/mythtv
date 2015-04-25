/*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_TEXTST_RENDER_H_)
#define _TEXTST_RENDER_H_

#include "textst.h"

#include "util/attributes.h"
#include "util/bits.h"

#include <stdint.h>

/*
 *
 */

typedef struct textst_render TEXTST_RENDER;

typedef struct {
  uint8_t  *mem;
  uint16_t  width;
  uint16_t  height;
  uint16_t  stride;
  uint8_t   argb;    /* Output buffer is ARGB (support for anti-aliasing) */
} TEXTST_BITMAP;

/*
 *
 */

BD_PRIVATE TEXTST_RENDER *textst_render_init(void);
BD_PRIVATE void           textst_render_free(TEXTST_RENDER **pp);

/*
 *
 */

BD_PRIVATE int textst_render_add_font(TEXTST_RENDER *p, void *data, size_t size);
BD_PRIVATE int textst_render_set_char_code(TEXTST_RENDER *p, int char_code);
BD_PRIVATE int textst_render(TEXTST_RENDER *p,
                             TEXTST_BITMAP *bmp,
                             const BD_TEXTST_REGION_STYLE *style,
                             const BD_TEXTST_DIALOG_REGION *region);

#endif /* _TEXTST_RENDER_H_ */
