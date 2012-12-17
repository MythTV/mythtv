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

#if !defined(_BD_IG_H_)
#define _BD_IG_H_

#include "pg.h"
#include "../bdnav/uo_mask_table.h"

#include <stdint.h>

struct bd_mobj_cmd_s;

/*
 *
 */

typedef struct bd_ig_button_s {
    uint16_t      id;

    uint16_t      numeric_select_value;
    uint8_t       auto_action_flag;

    uint16_t      x_pos;
    uint16_t      y_pos;

    /* neighbor info */
    uint16_t      upper_button_id_ref;
    uint16_t      lower_button_id_ref;
    uint16_t      left_button_id_ref;
    uint16_t      right_button_id_ref;

    /* normal state */
    uint16_t      normal_start_object_id_ref;
    uint16_t      normal_end_object_id_ref;
    uint8_t       normal_repeat_flag;

    /* selected state */
    uint8_t       selected_sound_id_ref;
    uint16_t      selected_start_object_id_ref;
    uint16_t      selected_end_object_id_ref;
    uint8_t       selected_repeat_flag;

    /* activated state */
    uint8_t       activated_sound_id_ref;
    uint16_t      activated_start_object_id_ref;
    uint16_t      activated_end_object_id_ref;

    /* navigation commands */
    uint16_t      num_nav_cmds;
    struct bd_mobj_cmd_s *nav_cmds;

} BD_IG_BUTTON;

typedef struct bd_ig_button_overlap_group_s {
    uint16_t      default_valid_button_id_ref;

    unsigned      num_buttons;
    BD_IG_BUTTON *button;

} BD_IG_BOG;

typedef struct bd_ig_effect_s {
    uint32_t      duration;        /* 90kHz ticks */
    uint8_t       palette_id_ref;

    unsigned      num_composition_objects;
    BD_PG_COMPOSITION_OBJECT *composition_object;

} BD_IG_EFFECT;

typedef struct bd_ig_effect_sequence_s {
    uint8_t       num_windows;
    BD_PG_WINDOW *window;

    uint8_t       num_effects;
    BD_IG_EFFECT *effect;

} BD_IG_EFFECT_SEQUENCE;

typedef struct bd_ig_page_s {
    uint8_t       id;
    uint8_t       version;

    BD_UO_MASK    uo_mask_table;

    BD_IG_EFFECT_SEQUENCE in_effects;
    BD_IG_EFFECT_SEQUENCE out_effects;

    uint8_t       animation_frame_rate_code;
    uint16_t      default_selected_button_id_ref;
    uint16_t      default_activated_button_id_ref;
    uint8_t       palette_id_ref;

    /* button overlap groups */
    unsigned      num_bogs;
    BD_IG_BOG    *bog;

} BD_IG_PAGE;

typedef struct bd_ig_interactive_composition_s {
    uint8_t       stream_model;
    uint8_t       ui_model;      /* 0 - always on, 1 - pop-up */

    uint64_t      composition_timeout_pts;
    uint64_t      selection_timeout_pts;
    uint32_t      user_timeout_duration;

    unsigned      num_pages;
    BD_IG_PAGE   *page;

} BD_IG_INTERACTIVE_COMPOSITION;

#define IG_UI_MODEL_ALWAYS_ON 0
#define IG_UI_MODEL_POPUP     1

/*
 * segment
 */

typedef struct bd_pg_interactive_s {
    int64_t       pts;

    BD_PG_VIDEO_DESCRIPTOR        video_descriptor;
    BD_PG_COMPOSITION_DESCRIPTOR  composition_descriptor;
    BD_IG_INTERACTIVE_COMPOSITION interactive_composition;
} BD_IG_INTERACTIVE;

#endif // _BD_IG_H_
