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

#if !defined(_BD_TEXTST_H_)
#define _BD_TEXTST_H_

#include "pg.h"

#include <stdint.h>


#define BD_TEXTST_FLOW_LEFT_RIGHT  1  /* Left-to-Right character progression, Top-to-Bottom line progression */
#define BD_TEXTST_FLOW_RIGHT_LEFT  2  /* Right-to-Left character progression, Top-to-Bottom line progression */
#define BD_TEXTST_FLOW_TOP_BOTTOM  3  /* Top-to-Bottom character progression, Right-to-Left line progression */

#define BD_TEXTST_HALIGN_LEFT   1
#define BD_TEXTST_HALIGN_CENTER 2
#define BD_TEXTST_HALIGN_RIGHT  3

#define BD_TEXTST_VALIGN_TOP    1
#define BD_TEXTST_VALIGN_MIDDLE 2
#define BD_TEXTST_VALIGN_BOTTOM 3

#define BD_TEXTST_FONT_OUTLINE_THIN   1
#define BD_TEXTST_FONT_OUTLINE_MEDIUM 2
#define BD_TEXTST_FONT_OUTLINE_THICK  3

#define BD_TEXTST_DATA_STRING      1
#define BD_TEXTST_DATA_FONT_ID     2
#define BD_TEXTST_DATA_FONT_STYLE  3
#define BD_TEXTST_DATA_FONT_SIZE   4
#define BD_TEXTST_DATA_FONT_COLOR  5
#define BD_TEXTST_DATA_NEWLINE     0x0a
#define BD_TEXTST_DATA_RESET_STYLE 0x0b


typedef struct {
    uint16_t xpos;
    uint16_t ypos;
    uint16_t width;
    uint16_t height;
} BD_TEXTST_RECT;

typedef struct {
    BD_TEXTST_RECT region;
    uint8_t        background_color; /* palette entry id ref */
} BD_TEXTST_REGION_INFO;

typedef struct {
    uint8_t bold : 1;
    uint8_t italic : 1;
    uint8_t outline_border : 1;
} BD_TEXTST_FONT_STYLE;

typedef struct {
    uint8_t               region_style_id;
    BD_TEXTST_REGION_INFO region_info;
    BD_TEXTST_RECT        text_box;          /* relative to region */
    uint8_t               text_flow;         /* BD_TEXTST_FLOW_* */
    uint8_t               text_halign;       /* BD_TEXTST_HALIGN_* */
    uint8_t               text_valign;       /* BD_TEXTST_VALIGN_* */
    uint8_t               line_space;
    uint8_t               font_id_ref;
    BD_TEXTST_FONT_STYLE  font_style;
    uint8_t               font_size;
    uint8_t               font_color;        /* palette entry id ref */
    uint8_t               outline_color;     /* palette entry id ref */
    uint8_t               outline_thickness; /* BD_TEXTST_FONT_OUTLINE_* */
} BD_TEXTST_REGION_STYLE;

typedef struct {
    uint8_t user_style_id;
    int16_t region_hpos_delta;
    int16_t region_vpos_delta;
    int16_t text_box_hpos_delta;
    int16_t text_box_vpos_delta;
    int16_t text_box_width_delta;
    int16_t text_box_height_delta;
    int8_t  font_size_delta;
    int8_t  line_space_delta;
} BD_TEXTST_USER_STYLE;

typedef struct {
    uint8_t type;  /* BD_TEXTST_DATA_* */

    union {
        uint8_t font_id_ref;
        uint8_t font_size;
        uint8_t font_color;
        struct {
            BD_TEXTST_FONT_STYLE style;
            uint8_t              outline_color;
            uint8_t              outline_thickness;
        } style;
        struct {
            uint8_t length;
            uint8_t string[1];
        } text;
    } data;
} BD_TEXTST_DATA;

typedef struct {
    uint8_t         continous_present_flag;
    uint8_t         forced_on_flag;
    uint8_t         region_style_id_ref;

    unsigned        elem_count;
    BD_TEXTST_DATA *elem; /* note: variable-sized elements */

    unsigned        line_count;
} BD_TEXTST_DIALOG_REGION;

/*
 * segments
 */

typedef struct {
    uint8_t                 player_style_flag;
    uint8_t                 region_style_count;
    uint8_t                 user_style_count;
    BD_TEXTST_REGION_STYLE *region_style;
    BD_TEXTST_USER_STYLE   *user_style;
    BD_PG_PALETTE_ENTRY     palette[256];
} BD_TEXTST_DIALOG_STYLE;

typedef struct {
    int64_t                  start_pts;
    int64_t                  end_pts;

    BD_PG_PALETTE_ENTRY     *palette_update;

    uint8_t                  region_count;
    BD_TEXTST_DIALOG_REGION  region[2];

} BD_TEXTST_DIALOG_PRESENTATION;

#endif // _BD_TEXTST_H_
