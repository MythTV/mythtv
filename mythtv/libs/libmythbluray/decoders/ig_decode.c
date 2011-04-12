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

#include "ig_decode.h"

#include "pg_decode.h"           // pg_decode_*()
#include "../hdmv/mobj_parse.h"  // mobj_parse_cmd()
#include "../bdnav/mpls_parse.h" // mpls_parse_uo()

#include "util/macro.h"
#include "util/logging.h"
#include "util/bits.h"

#include <string.h>
#include <stdlib.h>


static void _decode_button(BITBUFFER *bb, BD_IG_BUTTON *p)
{
    unsigned ii;

    p->id                   = bb_read(bb, 16);

    p->numeric_select_value = bb_read(bb, 16);
    p->auto_action_flag     = bb_read(bb, 1);
    bb_skip(bb, 7);

    p->x_pos                = bb_read(bb, 16);
    p->y_pos                = bb_read(bb, 16);

    p->upper_button_id_ref  = bb_read(bb, 16);
    p->lower_button_id_ref  = bb_read(bb, 16);
    p->left_button_id_ref   = bb_read(bb, 16);
    p->right_button_id_ref  = bb_read(bb, 16);

    p->normal_start_object_id_ref    = bb_read(bb, 16);
    p->normal_end_object_id_ref      = bb_read(bb, 16);
    p->normal_repeat_flag            = bb_read(bb, 1);
    bb_skip(bb, 7);

    p->selected_sound_id_ref         = bb_read(bb, 8);
    p->selected_start_object_id_ref  = bb_read(bb, 16);
    p->selected_end_object_id_ref    = bb_read(bb, 16);
    p->selected_repeat_flag          = bb_read(bb, 1);
    bb_skip(bb, 7);

    p->activated_sound_id_ref        = bb_read(bb, 8);
    p->activated_start_object_id_ref = bb_read(bb, 16);
    p->activated_end_object_id_ref   = bb_read(bb, 16);

    p->num_nav_cmds = bb_read(bb, 16);
    p->nav_cmds     = calloc(p->num_nav_cmds, sizeof(MOBJ_CMD));

    for (ii = 0; ii < p->num_nav_cmds; ii++) {
        uint8_t buf[12];
        bb_read_bytes(bb, buf, 12);

        mobj_parse_cmd(buf, &p->nav_cmds[ii]);
    }
}

static void _clean_button(BD_IG_BUTTON *p)
{
    X_FREE(p->nav_cmds);
}

static void _decode_bog(BITBUFFER *bb, BD_IG_BOG *p)
{
    unsigned ii;

    p->default_valid_button_id_ref = bb_read(bb, 16);

    p->num_buttons = bb_read(bb, 8);
    p->button      = calloc(p->num_buttons, sizeof(BD_IG_BUTTON));

    for (ii = 0; ii < p->num_buttons; ii++) {
        _decode_button(bb, &p->button[ii]);
    }
}

static void _clean_bog(BD_IG_BOG *p)
{
    unsigned ii;

    for (ii = 0; ii < p->num_buttons; ii++) {
        _clean_button(&p->button[ii]);
    }
    X_FREE(p->button);
}

static void _decode_effect(BITBUFFER *bb, BD_IG_EFFECT *p)
{
    unsigned ii;

    p->duration       = bb_read(bb, 24);
    p->palette_id_ref = bb_read(bb, 8);

    p->num_composition_objects = bb_read(bb, 8);
    p->composition_object      = calloc(p->num_composition_objects, sizeof(BD_PG_COMPOSITION_OBJECT));

    for (ii = 0; ii < p->num_composition_objects; ii++) {
        pg_decode_composition_object(bb, &p->composition_object[ii]);
    }
}

static void _clean_effect(BD_IG_EFFECT *p)
{
    X_FREE(p->composition_object);
}

static void _decode_effect_sequence(BITBUFFER *bb, BD_IG_EFFECT_SEQUENCE *p)
{
    unsigned ii;

    p->num_windows = bb_read(bb, 8);
    p->window      = calloc(p->num_windows, sizeof(BD_PG_WINDOW));

    for (ii = 0; ii < p->num_windows; ii++) {
        pg_decode_window(bb, &p->window[ii]);
    }

    p->num_effects = bb_read(bb, 8);
    p->effect      = calloc(p->num_effects, sizeof(BD_IG_EFFECT));

    for (ii = 0; ii < p->num_effects; ii++) {
        _decode_effect(bb, &p->effect[ii]);
    }
}

static void _clean_effect_sequence(BD_IG_EFFECT_SEQUENCE *p)
{
    unsigned ii;

    for (ii = 0; ii < p->num_effects; ii++) {
        _clean_effect(&p->effect[ii]);
    }
    X_FREE(p->effect);

    X_FREE(p->window);
}


static int _decode_uo_mask_table(BITBUFFER *bb, BD_UO_MASK *p)
{
    uint8_t buf[8];
    bb_read_bytes(bb, buf, 8);

    return mpls_parse_uo(buf, p);
}

static void _decode_page(BITBUFFER *bb, BD_IG_PAGE *p)
{
    unsigned ii;

    p->id      = bb_read(bb, 8);
    p->version = bb_read(bb, 8);

    _decode_uo_mask_table(bb, &p->uo_mask_table);

    _decode_effect_sequence(bb, &p->in_effects);
    _decode_effect_sequence(bb, &p->out_effects);

    p->animation_frame_rate_code       = bb_read(bb, 8);
    p->default_selected_button_id_ref  = bb_read(bb, 16);
    p->default_activated_button_id_ref = bb_read(bb, 16);
    p->palette_id_ref                  = bb_read(bb, 8);

    p->num_bogs = bb_read(bb, 8);
    p->bog      = calloc(p->num_bogs, sizeof(BD_IG_BOG));

    for (ii = 0; ii < p->num_bogs; ii++) {
        _decode_bog(bb, &p->bog[ii]);
    }
}

static void _clean_page(BD_IG_PAGE *p)
{
    unsigned ii;

    _clean_effect_sequence(&p->in_effects);
    _clean_effect_sequence(&p->out_effects);

    for (ii = 0; ii < p->num_bogs; ii++) {
        _clean_bog(&p->bog[ii]);
    }
    X_FREE(p->bog);
}

static inline uint64_t bb_read_u64(BITBUFFER *bb, int i_count)
{
    uint64_t result = 0;
    if (i_count > 32) {
        i_count -= 32;
        result = (uint64_t)bb_read(bb, 32) << i_count;
    }
    result |= bb_read(bb, i_count);
    return result;
}

static int _decode_interactive_composition(BITBUFFER *bb, BD_IG_INTERACTIVE_COMPOSITION *p)
{
    unsigned ii;

    uint32_t data_len = bb_read(bb, 24);
    uint32_t buf_len  = bb->p_end - bb->p;
    if (data_len != buf_len) {
        BD_DEBUG(DBG_DECODE, "ig_decode_interactive(): buffer size mismatch (expected %d, have %d)\n", data_len, buf_len);
        return 0;
    }

    p->stream_model = bb_read(bb, 1);
    p->ui_model     = bb_read(bb, 1);
    bb_skip(bb, 6);

    if (p->stream_model == 0) {
        bb_skip(bb, 7);
        p->composition_timeout_pts = bb_read_u64(bb, 33);
        bb_skip(bb, 7);
        p->selection_timeout_pts = bb_read_u64(bb, 33);
    }

    p->user_timeout_duration = bb_read(bb, 24);

    p->num_pages = bb_read(bb, 8);
    p->page      = calloc(p->num_pages, sizeof(BD_IG_PAGE));

    for (ii = 0; ii < p->num_pages; ii++) {
        _decode_page(bb, &p->page[ii]);
    }

  return 1;
}

static void _clean_interactive_composition(BD_IG_INTERACTIVE_COMPOSITION *p)
{
    unsigned ii;

    for (ii = 0; ii < p->num_pages; ii++) {
        _clean_page(&p->page[ii]);
    }
    X_FREE(p->page);
}

/*
 * segment
 */

int ig_decode_interactive(BITBUFFER *bb, BD_IG_INTERACTIVE *p)
{
    BD_PG_SEQUENCE_DESCRIPTOR sd;

    pg_decode_video_descriptor(bb, &p->video_descriptor);
    pg_decode_composition_descriptor(bb, &p->composition_descriptor);
    pg_decode_sequence_descriptor(bb, &sd);

    if (!sd.first_in_seq) {
        BD_DEBUG(DBG_DECODE, "ig_decode_interactive(): not first in seq\n");
        return 0;
    }
    if (!sd.last_in_seq) {
        BD_DEBUG(DBG_DECODE, "ig_decode_interactive(): not last in seq\n");
        return 0;
    }
    if (!bb_is_align(bb, 0x07)) {
        BD_DEBUG(DBG_DECODE, "ig_decode_interactive(): alignment error\n");
        return 0;
    }

    return _decode_interactive_composition(bb, &p->interactive_composition);
}

void ig_clean_interactive(BD_IG_INTERACTIVE *p)
{
    _clean_interactive_composition(&p->interactive_composition);
}

void ig_free_interactive(BD_IG_INTERACTIVE **p)
{
    if (p && *p) {
        _clean_interactive_composition(&(*p)->interactive_composition);
        X_FREE(*p);
    }
}
