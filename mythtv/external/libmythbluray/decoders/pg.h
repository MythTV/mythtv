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

#if !defined(_BD_PG_H_)
#define _BD_PG_H_

#include "overlay.h"

#include <stdint.h>

typedef struct {
    uint16_t video_width;
    uint16_t video_height;
    uint8_t  frame_rate;
} BD_PG_VIDEO_DESCRIPTOR;

typedef struct {
    uint16_t number;
    uint8_t  state;
} BD_PG_COMPOSITION_DESCRIPTOR;

typedef struct {
    uint8_t first_in_seq;
    uint8_t last_in_seq;
} BD_PG_SEQUENCE_DESCRIPTOR;

typedef struct {
    uint8_t  id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} BD_PG_WINDOW;

typedef struct {
    uint16_t object_id_ref;
    uint8_t  window_id_ref;
    uint8_t  forced_on_flag;

    uint16_t x;
    uint16_t y;

    uint8_t  crop_flag;
    uint16_t crop_x;
    uint16_t crop_y;
    uint16_t crop_w;
    uint16_t crop_h;
} BD_PG_COMPOSITION_OBJECT;

typedef struct {
    int64_t pts;

    uint8_t id;
    uint8_t version;

    BD_PG_PALETTE_ENTRY entry[256];
} BD_PG_PALETTE;

typedef struct {
    int64_t pts;

    uint16_t id;
    uint8_t  version;

    uint16_t width;
    uint16_t height;

    BD_PG_RLE_ELEM *img;

} BD_PG_OBJECT;

typedef struct {
    int64_t       pts;

    BD_PG_VIDEO_DESCRIPTOR        video_descriptor;
    BD_PG_COMPOSITION_DESCRIPTOR  composition_descriptor;

    uint8_t       palette_update_flag;
    uint8_t       palette_id_ref;

    unsigned                  num_composition_objects;
    BD_PG_COMPOSITION_OBJECT *composition_object;

} BD_PG_COMPOSITION;

typedef struct {
    int64_t       pts;

    unsigned      num_windows;
    BD_PG_WINDOW *window;
} BD_PG_WINDOWS;

#endif // _BD_PG_H_
