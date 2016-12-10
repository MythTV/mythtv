/*
 * This file is part of libbluray
 * Copyright (C) 2010-2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "graphics_controller.h"

#include "graphics_processor.h"
#include "hdmv_pids.h"
#include "ig.h"
#include "overlay.h"
#include "textst_render.h"
#include "rle.h"

#include "util/macro.h"
#include "util/logging.h"
#include "util/mutex.h"
#include "util/time.h"

#include "../register.h"
#include "../keys.h"

#ifdef _WIN32
/* mingw: PRId64 seems to expands to %d without stdio.h ... */
#include <stdio.h>
#endif

#include <inttypes.h>
#include <string.h>

#define GC_ERROR(...) BD_DEBUG(DBG_GC | DBG_CRIT, __VA_ARGS__)
#define GC_TRACE(...) BD_DEBUG(DBG_GC,            __VA_ARGS__)

/*
 *
 */

typedef struct {
    uint16_t enabled_button;  /* enabled button id */
    uint16_t x, y, w, h;      /* button rect on overlay plane (if drawn) */
    int      visible_object_id; /* id of currently visible object */
    int      animate_indx;    /* currently showing object index of animated button, < 0 for static buttons */
    int      effect_running;  /* single-loop animation not yet complete */
} BOG_DATA;

struct graphics_controller_s {

    BD_REGISTERS   *regs;

    BD_MUTEX        mutex;

    /* overlay output */
    void           *overlay_proc_handle;
    void          (*overlay_proc)(void *, const struct bd_overlay_s * const);

    /* state */
    unsigned        ig_open;
    unsigned        ig_drawn;
    unsigned        ig_dirty;
    unsigned        pg_open;
    unsigned        pg_drawn;
    unsigned        pg_dirty;
    unsigned        popup_visible;
    unsigned        valid_mouse_position;
    unsigned        auto_action_triggered;
    BOG_DATA       *bog_data;
    BOG_DATA       *saved_bog_data;
    BD_UO_MASK      page_uo_mask;

    /* page effects */
    unsigned               effect_idx;
    BD_IG_EFFECT_SEQUENCE *in_effects;
    BD_IG_EFFECT_SEQUENCE *out_effects;
    int64_t                next_effect_time; /* 90 kHz */

    /* timers */
    int64_t                user_timeout;

    /* animated buttons */
    unsigned               frame_interval;
    unsigned               button_effect_running;
    unsigned               button_animation_running;

    /* data */
    PG_DISPLAY_SET *pgs;
    PG_DISPLAY_SET *igs;
    PG_DISPLAY_SET *tgs;  /* TextST */

    /* */
    GRAPHICS_PROCESSOR *pgp;
    GRAPHICS_PROCESSOR *igp;
    GRAPHICS_PROCESSOR *tgp;  /* TextST */

    /* */
    TEXTST_RENDER  *textst_render;
    int             next_dialog_idx;
    int             textst_user_style;
};

/*
 * object lookup
 */

static BD_PG_OBJECT *_find_object(PG_DISPLAY_SET *s, unsigned object_id)
{
    unsigned ii;

    for (ii = 0; ii < s->num_object; ii++) {
        if (s->object[ii].id == object_id) {
            return &s->object[ii];
        }
    }

    return NULL;
}

static BD_PG_PALETTE *_find_palette(PG_DISPLAY_SET *s, unsigned palette_id)
{
    unsigned ii;

    for (ii = 0; ii < s->num_palette; ii++) {
        if (s->palette[ii].id == palette_id) {
            return &s->palette[ii];
        }
    }

    return NULL;
}

static BD_IG_BUTTON *_find_button_bog(BD_IG_BOG *bog, unsigned button_id)
{
    unsigned ii;

    for (ii = 0; ii < bog->num_buttons; ii++) {
        if (bog->button[ii].id == button_id) {
            return &bog->button[ii];
        }
    }

    return NULL;
}

static BD_IG_BUTTON *_find_button_page(BD_IG_PAGE *page, unsigned button_id, unsigned *bog_idx)
{
    unsigned ii;

    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BUTTON *button = _find_button_bog(&page->bog[ii], button_id);
        if (button) {
            if (bog_idx) {
                *bog_idx = ii;
            }
            return button;
        }
    }

    return NULL;
}

static BD_IG_PAGE *_find_page(BD_IG_INTERACTIVE_COMPOSITION *c, unsigned page_id)
{
    unsigned ii;

    for (ii = 0; ii < c->num_pages; ii++) {
        if (c->page[ii].id == page_id) {
            return &c->page[ii];
        }
    }

    return NULL;
}

enum { BTN_NORMAL, BTN_SELECTED, BTN_ACTIVATED };

static BD_PG_OBJECT *_find_object_for_button(PG_DISPLAY_SET *s,
                                             BD_IG_BUTTON *button, int state,
                                             BOG_DATA *bog_data)
{
    BD_PG_OBJECT *object   = NULL;
    unsigned object_id     = 0xffff;
    unsigned object_id_end = 0xffff;
    unsigned repeat        = 0;

    switch (state) {
        case BTN_NORMAL:
            object_id     = button->normal_start_object_id_ref;
            object_id_end = button->normal_end_object_id_ref;
            repeat        = button->normal_repeat_flag;
            break;
        case BTN_SELECTED:
            object_id     = button->selected_start_object_id_ref;
            object_id_end = button->selected_end_object_id_ref;
            repeat        = button->selected_repeat_flag;
            break;
        case BTN_ACTIVATED:
            object_id     = button->activated_start_object_id_ref;
            object_id_end = button->activated_end_object_id_ref;
            break;
    }

    if (bog_data) {
        bog_data->effect_running = 0;
        if (bog_data->animate_indx >= 0) {
            int range = object_id_end - object_id;

            if (range > 0 && object_id < 0xffff && object_id_end < 0xffff) {
                GC_TRACE("animate button #%d: animate_indx %d, range %d, repeat %d\n",
                         button->id, bog_data->animate_indx, range, repeat);

                object_id += bog_data->animate_indx % (range + 1);
                bog_data->animate_indx++;
                if (!repeat) {
                    if (bog_data->animate_indx > range) {
                        /* terminate animation to the last object */
                        bog_data->animate_indx = -1;
                    } else {
                        bog_data->effect_running = 1;
                    }
                }
            } else {
                /* no animation for this button */
                bog_data->animate_indx = -1;
            }
        } else {
            if (object_id_end < 0xfffe) {
                object_id = object_id_end;
            }
        }
    }

    object = _find_object(s, object_id);

    return object;
}

static BD_TEXTST_REGION_STYLE *_find_region_style(BD_TEXTST_DIALOG_STYLE *p, unsigned region_style_id)
{
    unsigned ii;

    for (ii = 0; ii < p->region_style_count; ii++) {
        if (p->region_style[ii].region_style_id == region_style_id) {
            return &p->region_style[ii];
        }
    }

    return NULL;
}

/*
 * util
 */

static int _areas_overlap(BOG_DATA *a, BOG_DATA *b)
{
    return !(a->x + a->w <= b->x        ||
             a->x        >= b->x + b->w ||
             a->y + a->h <= b->y        ||
             a->y        >= b->y + b->h);
}

static int _is_button_enabled(GRAPHICS_CONTROLLER *gc, BD_IG_PAGE *page, unsigned button_id)
{
    unsigned ii;
    for (ii = 0; ii < page->num_bogs; ii++) {
        if (gc->bog_data[ii].enabled_button == button_id) {
            return 1;
        }
    }
    return 0;
}

static uint16_t _find_selected_button_id(GRAPHICS_CONTROLLER *gc)
{
    /* executed when playback condition changes (ex. new page, popup-on, ...) */
    PG_DISPLAY_SET *s         = gc->igs;
    BD_IG_PAGE     *page      = NULL;
    unsigned        page_id   = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        button_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);
    unsigned        ii;

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        GC_TRACE("_find_selected_button_id(): unknown page #%d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return 0xffff;
    }

    /* run 5.9.8.3 */

    /* 1) always use page->default_selected_button_id_ref if it is valid */
    if (_find_button_page(page, page->default_selected_button_id_ref, NULL) &&
        _is_button_enabled(gc, page, page->default_selected_button_id_ref)) {

        GC_TRACE("_find_selected_button_id() -> default #%d\n", page->default_selected_button_id_ref);
        return page->default_selected_button_id_ref;
    }

    /* 2) fallback to current PSR10 value if it is valid */
    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BOG *bog = &page->bog[ii];
        uint16_t   enabled_button = gc->bog_data[ii].enabled_button;

        if (button_id == enabled_button) {
            if (_find_button_bog(bog, enabled_button)) {
                GC_TRACE("_find_selected_button_id() -> PSR10 #%d\n", enabled_button);
                return enabled_button;
            }
        }
    }

    /* 3) fallback to find first valid_button_id_ref from page */
    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BOG *bog = &page->bog[ii];
        uint16_t   enabled_button = gc->bog_data[ii].enabled_button;

        if (_find_button_bog(bog, enabled_button)) {
            GC_TRACE("_find_selected_button_id() -> first valid #%d\n", enabled_button);
            return enabled_button;
        }
    }

    GC_TRACE("_find_selected_button_id(): not found -> 0xffff\n");
    return 0xffff;
}

static void _reset_user_timeout(GRAPHICS_CONTROLLER *gc)
{
    gc->user_timeout = 0;

    if (gc->igs->ics->interactive_composition.ui_model == IG_UI_MODEL_POPUP ||
        bd_psr_read(gc->regs, PSR_MENU_PAGE_ID) != 0) {

        gc->user_timeout = gc->igs->ics->interactive_composition.user_timeout_duration;
        if (gc->user_timeout) {
            gc->user_timeout += bd_get_scr();
        }
    }
}

static int _save_page_state(GRAPHICS_CONTROLLER *gc)
{
    if (!gc->bog_data) {
        GC_TRACE("_save_page_state(): no bog data !\n");
        return -1;
    }
    if (!gc->igs || !gc->igs->ics) {
        GC_TRACE("_save_page_state(): no IG composition\n");
        return -1;
    }

    PG_DISPLAY_SET *s       = gc->igs;
    BD_IG_PAGE     *page    = NULL;
    unsigned        page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        ii;

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        GC_ERROR("_save_page_state(): unknown page #%d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return -1;
    }

    /* copy enabled button state, clear draw state */

    X_FREE(gc->saved_bog_data);
    gc->saved_bog_data = calloc(page->num_bogs, sizeof(*gc->saved_bog_data));
    if (!gc->saved_bog_data) {
        GC_ERROR("_save_page_state(): out of memory\n");
        return -1;
    }

    for (ii = 0; ii < page->num_bogs; ii++) {
        gc->saved_bog_data[ii].enabled_button = gc->bog_data[ii].enabled_button;
        gc->saved_bog_data[ii].animate_indx   = gc->bog_data[ii].animate_indx >= 0 ? 0 : -1;
    }

    return 1;
}

static int _restore_page_state(GRAPHICS_CONTROLLER *gc)
{
    gc->in_effects  = NULL;
    gc->out_effects = NULL;

    if (gc->saved_bog_data) {
        if (gc->bog_data) {
            GC_ERROR("_restore_page_state(): bog data already exists !\n");
            X_FREE(gc->bog_data);
        }
        gc->bog_data       = gc->saved_bog_data;
        gc->saved_bog_data = NULL;

        return 1;
    }
    return -1;
}

static void _reset_page_state(GRAPHICS_CONTROLLER *gc)
{
    PG_DISPLAY_SET *s       = gc->igs;
    BD_IG_PAGE     *page    = NULL;
    unsigned        page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        ii;

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        GC_ERROR("_reset_page_state(): unknown page #%d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return;
    }

    size_t size = page->num_bogs * sizeof(*gc->bog_data);
    gc->bog_data = realloc(gc->bog_data, size);

    memset(gc->bog_data, 0, size);

    for (ii = 0; ii < page->num_bogs; ii++) {
        gc->bog_data[ii].enabled_button = page->bog[ii].default_valid_button_id_ref;
        gc->bog_data[ii].animate_indx   = 0;
        gc->bog_data[ii].visible_object_id = -1;
    }

    /* animation frame rate */
    static const unsigned frame_interval[8] = {
        0,
        90000 / 1001 * 24,
        90000 / 1000 * 24,
        90000 / 1000 * 25,
        90000 / 1001 * 30,
        90000 / 1000 * 50,
        90000 / 1001 * 60,
    };
    gc->frame_interval = frame_interval[s->ics->video_descriptor.frame_rate] * (page->animation_frame_rate_code + 1);

    /* effects */
    gc->effect_idx  = 0;
    gc->in_effects  = NULL;
    gc->out_effects = NULL;

    /* timers */
    _reset_user_timeout(gc);
}

/*
 * overlay operations
 */

#if defined __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

static void _open_osd(GRAPHICS_CONTROLLER *gc, int plane,
                      unsigned x0, unsigned y0,
                      unsigned width, unsigned height)
{
    if (gc->overlay_proc) {
        BD_OVERLAY ov = {0};
        ov.cmd          = BD_OVERLAY_INIT;
        ov.pts          = -1;
        ov.plane        = plane;
        ov.x            = x0;
        ov.y            = y0;
        ov.w            = width;
        ov.h            = height;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);

        if (plane == BD_OVERLAY_IG) {
            gc->ig_open = 1;
        } else {
            gc->pg_open = 1;
        }
    }
}

static void _close_osd(GRAPHICS_CONTROLLER *gc, int plane)
{
    if (gc->overlay_proc) {
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_CLOSE;
        ov.pts     = -1;
        ov.plane   = plane;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }

    if (plane == BD_OVERLAY_IG) {
        gc->ig_open = 0;
        gc->ig_drawn = 0;
    } else {
        gc->pg_open = 0;
        gc->pg_drawn = 0;
    }
}

static void _flush_osd(GRAPHICS_CONTROLLER *gc, int plane, int64_t pts)
{
    if (gc->overlay_proc) {
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_FLUSH;
        ov.pts     = pts;
        ov.plane   = plane;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }
}

static void _hide_osd(GRAPHICS_CONTROLLER *gc, int plane)
{
    if (gc->overlay_proc) {
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_HIDE;
        ov.plane   = plane;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }
}

static void _clear_osd_area(GRAPHICS_CONTROLLER *gc, int plane, int64_t pts,
                            uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (gc->overlay_proc) {
        /* wipe area */
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_WIPE;
        ov.pts     = pts;
        ov.plane   = plane;
        ov.x       = x;
        ov.y       = y;
        ov.w       = w;
        ov.h       = h;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }
}

static void _clear_osd(GRAPHICS_CONTROLLER *gc, int plane)
{
    if (gc->overlay_proc) {
        /* clear plane */
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_CLEAR;
        ov.pts     = -1;
        ov.plane   = plane;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }

    if (plane == BD_OVERLAY_IG) {
        gc->ig_drawn      = 0;
    } else {
        gc->pg_drawn      = 0;
    }
}

static void _clear_bog_area(GRAPHICS_CONTROLLER *gc, BOG_DATA *bog_data)
{
    if (gc->ig_drawn && bog_data->w && bog_data->h) {

        _clear_osd_area(gc, BD_OVERLAY_IG, -1, bog_data->x, bog_data->y, bog_data->w, bog_data->h);

        bog_data->x = bog_data->y = bog_data->w = bog_data->h = 0;
        bog_data->visible_object_id = -1;

        gc->ig_dirty = 1;
    }
}

static void _render_object(GRAPHICS_CONTROLLER *gc,
                           int64_t pts, unsigned plane,
                           uint16_t x, uint16_t y,
                           BD_PG_OBJECT *object,
                           BD_PG_PALETTE *palette)
{
    if (gc->overlay_proc) {
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_DRAW;
        ov.pts     = pts;
        ov.plane   = plane;
        ov.x       = x;
        ov.y       = y;
        ov.w       = object->width;
        ov.h       = object->height;
        ov.palette = palette->entry;
        ov.img     = object->img;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }
}

static void _render_composition_object(GRAPHICS_CONTROLLER *gc,
                                       int64_t pts, unsigned plane,
                                       BD_PG_COMPOSITION_OBJECT *cobj,
                                       BD_PG_OBJECT *object,
                                       BD_PG_PALETTE *palette,
                                       int palette_update_flag)
{
    if (gc->overlay_proc) {
        BD_PG_RLE_ELEM *cropped_img = NULL;
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_DRAW;
        ov.pts     = pts;
        ov.plane   = plane;
        ov.x       = cobj->x;
        ov.y       = cobj->y;
        ov.w       = object->width;
        ov.h       = object->height;
        ov.palette = palette->entry;
        ov.img     = object->img;

        if (cobj->crop_flag) {
            if (cobj->crop_x || cobj->crop_y || cobj->crop_w != object->width) {
                cropped_img = rle_crop_object(object->img, object->width,
                                              cobj->crop_x, cobj->crop_y, cobj->crop_w, cobj->crop_h);
                if (!cropped_img) {
                    BD_DEBUG(DBG_DECODE | DBG_CRIT, "Error cropping PG object\n");
                    return;
                }
                ov.img = cropped_img;
            }
            ov.w  = cobj->crop_w;
            ov.h  = cobj->crop_h;
        }

        ov.palette_update_flag = palette_update_flag;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);

        bd_refcnt_dec(cropped_img);
    }
}

static void _render_rle(GRAPHICS_CONTROLLER *gc,
                        int64_t pts,
                        BD_PG_RLE_ELEM *img,
                        uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height,
                        BD_PG_PALETTE_ENTRY *palette)
{
    if (gc->overlay_proc) {
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_DRAW;
        ov.pts     = pts;
        ov.plane   = BD_OVERLAY_PG;
        ov.x       = x;
        ov.y       = y;
        ov.w       = width;
        ov.h       = height;
        ov.palette = palette;
        ov.img     = img;

        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }
}

/*
 * page selection and IG effects
 */

static void _select_button(GRAPHICS_CONTROLLER *gc, uint32_t button_id)
{
    BD_IG_PAGE     *page       = NULL;
    unsigned        page_id    = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        bog_idx    = 0;

    /* reset animation */
    page = _find_page(&gc->igs->ics->interactive_composition, page_id);
    if (page && _find_button_page(page, button_id, &bog_idx)) {
        gc->bog_data[bog_idx].animate_indx = 0;
        gc->next_effect_time = bd_get_scr();
    }

    /* select page */
    bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, button_id);
    gc->auto_action_triggered = 0;
}

static void _select_page(GRAPHICS_CONTROLLER *gc, uint16_t page_id, int out_effects)
{
    unsigned cur_page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    BD_IG_PAGE *page = NULL;

    bd_psr_write(gc->regs, PSR_MENU_PAGE_ID, page_id);

    _reset_page_state(gc);

    uint16_t button_id = _find_selected_button_id(gc);
    _select_button(gc, button_id);

    gc->valid_mouse_position = 0;

    if (out_effects) {
        page = _find_page(&gc->igs->ics->interactive_composition, cur_page_id);
        if (page && page->out_effects.num_effects) {
            gc->next_effect_time = bd_get_scr();
            gc->out_effects = &page->out_effects;
        }
    }

    page = _find_page(&gc->igs->ics->interactive_composition, page_id);
    if (page && page->in_effects.num_effects) {
        gc->next_effect_time = bd_get_scr();
        gc->in_effects = &page->in_effects;
    }

    if (gc->ig_open && !gc->out_effects) {
        _clear_osd(gc, BD_OVERLAY_IG);
    }
}

static void _gc_reset(GRAPHICS_CONTROLLER *gc)
{
    if (gc->pg_open) {
        _close_osd(gc, BD_OVERLAY_PG);
    }
    if (gc->ig_open) {
        _close_osd(gc, BD_OVERLAY_IG);
    }

    gc->popup_visible = 0;
    gc->valid_mouse_position = 0;
    gc->page_uo_mask = bd_empty_uo_mask();

    graphics_processor_free(&gc->igp);
    graphics_processor_free(&gc->pgp);
    graphics_processor_free(&gc->tgp);

    pg_display_set_free(&gc->pgs);
    pg_display_set_free(&gc->igs);
    pg_display_set_free(&gc->tgs);

    textst_render_free(&gc->textst_render);
    gc->next_dialog_idx = 0;
    gc->textst_user_style = -1;

    X_FREE(gc->bog_data);
}

/*
 * register hook
 */
static void _process_psr_event(void *handle, BD_PSR_EVENT *ev)
{
    GRAPHICS_CONTROLLER *gc = (GRAPHICS_CONTROLLER *)handle;

    if (ev->ev_type == BD_PSR_SAVE) {
        BD_DEBUG(DBG_GC, "PSR SAVE event\n");

        /* save menu page state */
        bd_mutex_lock(&gc->mutex);
        _save_page_state(gc);
        bd_mutex_unlock(&gc->mutex);

        return;
    }

    if (ev->ev_type == BD_PSR_RESTORE) {
        switch (ev->psr_idx) {

            case PSR_SELECTED_BUTTON_ID:
              return;

            case PSR_MENU_PAGE_ID:
                /* restore menus */
                bd_mutex_lock(&gc->mutex);
                _restore_page_state(gc);
                bd_mutex_unlock(&gc->mutex);
                return;

            default:
                /* others: ignore */
                return;
        }
    }
}

/*
 * init / free
 */

GRAPHICS_CONTROLLER *gc_init(BD_REGISTERS *regs, void *handle, gc_overlay_proc_f func)
{
    GRAPHICS_CONTROLLER *p = calloc(1, sizeof(*p));
    if (!p) {
        GC_ERROR("gc_init(): out of memory\n");
        return NULL;
    }

    p->regs = regs;

    p->overlay_proc_handle = handle;
    p->overlay_proc        = func;

    bd_mutex_init(&p->mutex);

    bd_psr_register_cb(regs, _process_psr_event, p);

    p->textst_user_style = -1;

    return p;
}

void gc_free(GRAPHICS_CONTROLLER **p)
{
    if (p && *p) {

        GRAPHICS_CONTROLLER *gc = *p;

        bd_psr_unregister_cb(gc->regs, _process_psr_event, gc);

        _gc_reset(gc);

        if (gc->overlay_proc) {
            gc->overlay_proc(gc->overlay_proc_handle, NULL);
        }

        bd_mutex_destroy(&gc->mutex);

        X_FREE(gc->saved_bog_data);

        X_FREE(*p);
    }
}

/*
 * graphics stream input
 */

int gc_decode_ts(GRAPHICS_CONTROLLER *gc, uint16_t pid, uint8_t *block, unsigned num_blocks, int64_t stc)
{
    if (!gc) {
        GC_TRACE("gc_decode_ts(): no graphics controller\n");
        return -1;
    }

    if (IS_HDMV_PID_IG(pid)) {
        /* IG stream */

        if (!gc->igp) {
            gc->igp = graphics_processor_init();
            if (!gc->igp) {
                return -1;
            }
        }

        bd_mutex_lock(&gc->mutex);

        if (!graphics_processor_decode_ts(gc->igp, &gc->igs,
                                          pid, block, num_blocks,
                                          stc)) {
            /* no new complete display set */
            bd_mutex_unlock(&gc->mutex);
            return 0;
        }

        if (!gc->igs || !gc->igs->complete) {
            bd_mutex_unlock(&gc->mutex);
            return 0;
        }

        /* TODO: */
        if (gc->igs->ics) {
            if (gc->igs->ics->interactive_composition.composition_timeout_pts > 0) {
                GC_TRACE("gc_decode_ts(): IG composition_timeout_pts not implemented\n");
            }
            if (gc->igs->ics->interactive_composition.selection_timeout_pts) {
                GC_TRACE("gc_decode_ts(): IG selection_timeout_pts not implemented\n");
            }
            if (gc->igs->ics->interactive_composition.user_timeout_duration) {
                GC_TRACE("gc_decode_ts(): IG user_timeout_duration %d\n",
                         gc->igs->ics->interactive_composition.user_timeout_duration);
            }
        }

        bd_mutex_unlock(&gc->mutex);

        return 1;
    }

    else if (IS_HDMV_PID_PG(pid)) {
        /* PG stream */
        if (!gc->pgp) {
            gc->pgp = graphics_processor_init();
            if (!gc->pgp) {
                return -1;
            }
        }
        graphics_processor_decode_ts(gc->pgp, &gc->pgs,
                                     pid, block, num_blocks,
                                     stc);

        if (!gc->pgs || !gc->pgs->complete) {
            return 0;
        }

        return 1;
    }

    else if (IS_HDMV_PID_TEXTST(pid)) {
        /* TextST stream */
        if (!gc->tgp) {
            gc->tgp = graphics_processor_init();
            if (!gc->tgp) {
                return -1;
            }
        }
        graphics_processor_decode_ts(gc->tgp, &gc->tgs,
                                     pid, block, num_blocks,
                                     stc);

        if (!gc->tgs || !gc->tgs->complete) {
            return 0;
        }

        return 1;
    }

    return -1;
}

/*
 * TextST rendering
 */

static int _textst_style_select(GRAPHICS_CONTROLLER *p, int user_style_idx)
{
    p->textst_user_style = user_style_idx;

    GC_ERROR("User style selection not implemented\n");
    return -1;
}

int gc_add_font(GRAPHICS_CONTROLLER *p, void *data, size_t size)
{
    if (!p) {
        return -1;
    }

    if (!data) {
        textst_render_free(&p->textst_render);
        return 0;
    }

    if (!p->textst_render) {
        p->textst_render = textst_render_init();
        if (!p->textst_render) {
            return -1;
        }
    }

    return textst_render_add_font(p->textst_render, data, size);
}

static int _render_textst_region(GRAPHICS_CONTROLLER *p, int64_t pts, BD_TEXTST_REGION_STYLE *style, TEXTST_BITMAP *bmp,
                                 BD_PG_PALETTE_ENTRY *palette)
{
    unsigned bmp_y;
    uint16_t y;
    RLE_ENC  rle;

    if (rle_begin(&rle) < 0) {
        return -1;
    }

    for (y = 0, bmp_y = 0; y < style->region_info.region.height; y++) {
        if (y < style->text_box.ypos || y >= style->text_box.ypos + style->text_box.height) {
            if (rle_add_bite(&rle, style->region_info.background_color, style->region_info.region.width) < 0)
                break;
        } else {
            if (rle_add_bite(&rle, style->region_info.background_color, style->text_box.xpos) < 0)
                break;
            if (rle_compress_chunk(&rle, bmp->mem + bmp->stride * bmp_y, bmp->width) < 0)
                break;
            bmp_y++;
            if (rle_add_bite(&rle, style->region_info.background_color,
                             style->region_info.region.width - style->text_box.width - style->text_box.xpos) < 0)
                break;
        }

        if (rle_add_eol(&rle) < 0)
            break;
    }

    BD_PG_RLE_ELEM *img = rle_get(&rle);
    if (img) {
        _render_rle(p, pts, img,
                    style->region_info.region.xpos, style->region_info.region.ypos,
                    style->region_info.region.width, style->region_info.region.height,
                    palette);
    } else {
        BD_DEBUG(DBG_DECODE | DBG_CRIT, "Error encoding Text Subtitle region\n");
    }

    rle_end(&rle);

    return 0;
}

static int _render_textst(GRAPHICS_CONTROLLER *p, uint32_t stc, GC_NAV_CMDS *cmds)
{
    BD_TEXTST_DIALOG_PRESENTATION *dialog = NULL;
    PG_DISPLAY_SET *s   = p->tgs;
    int64_t         now = ((int64_t)stc) << 1;
    unsigned ii, jj;

    if (!s || !s->dialog || !s->style) {
        GC_ERROR("_render_textst(): no TextST decoded\n");
        return -1;
    }
    if (!p->textst_render) {
        GC_ERROR("_render_textst(): no TextST renderer (missing fonts ?)\n");
        return -1;
    }

    dialog = s->dialog;

    /* loop over all matching dialogs */
    for (ii = p->next_dialog_idx; ii < s->num_dialog; ii++) {

        /* next dialog too far in future ? */
        if (now < 1 || dialog[ii].start_pts >= now + 90000) {
            GC_TRACE("_render_textst(): next event #%d in %"PRId64" seconds (pts %"PRId64")\n",
                     ii, (dialog[ii].start_pts - now)/90000, dialog[ii].start_pts);
            if (cmds) {
                cmds->wakeup_time = (uint32_t)(dialog[ii].start_pts / 2);
            }
            return 1;
        }

        p->next_dialog_idx = ii + 1;

        /* too late ? */
        if (dialog[ii].start_pts < now - 45000) {
            GC_TRACE("_render_textst(): not showing #%d (start time passed)\n",ii);
            continue;
        }
        if (dialog[ii].end_pts < now) {
            GC_TRACE("_render_textst(): not showing #%d (hide time passed)\n",ii);
            continue;
        }

        if (dialog[ii].palette_update) {
            GC_ERROR("_render_textst(): Palette update not implemented\n");
            continue;
        }

        GC_TRACE("_render_textst(): rendering dialog #%d (pts %"PRId64", diff %"PRId64"\n",
                 ii, dialog[ii].start_pts, dialog[ii].start_pts - now);


        if (!dialog[ii].region_count) {
            continue;
        }

        /* TODO: */
        if (dialog[ii].region_count > 1) {
            GC_ERROR("_render_textst(): Multiple regions not supported\n");
        }

        /* open PG overlay */
        if (!p->pg_open) {
            _open_osd(p, BD_OVERLAY_PG, 0, 0, 1920, 1080);
        }

        /* render all regions */
        for (jj = 0; jj < dialog[ii].region_count; jj++) {

            BD_TEXTST_DIALOG_REGION *region = &dialog[ii].region[jj];
            BD_TEXTST_REGION_STYLE  *style = NULL;

            // TODO:
            if (region->continous_present_flag) {
                GC_ERROR("_render_textst(): continous_present_flag: not implemented\n");
            }
            if (region->forced_on_flag) {
                GC_ERROR("_render_textst(): forced_on_flag: not implemented\n");
            }

            style = _find_region_style(s->style, region->region_style_id_ref);
            if (!style) {
                GC_ERROR("_render_textst: region style #%d not found\n", region->region_style_id_ref);
                continue;
            }

            TEXTST_BITMAP bmp = {NULL, style->text_box.width, style->text_box.height, style->text_box.width, 0};
            bmp.mem = malloc((size_t)bmp.width * bmp.height);
            if (bmp.mem) {
                memset(bmp.mem, style->region_info.background_color, (size_t)bmp.width * bmp.height);

                textst_render(p->textst_render, &bmp, style, region);

                _render_textst_region(p, dialog[ii].start_pts, style, &bmp, s->style->palette);

                X_FREE(bmp.mem);
            } else {
                GC_ERROR("_render_textst(): out of memory\n");
            }
        }

        /* commit changes */
        _flush_osd(p, BD_OVERLAY_PG, dialog[ii].start_pts);

        /* detect overlapping dialogs (not allowed) */
        if (ii < s->num_dialog - 1) {
            if (dialog[ii + 1].start_pts < dialog[ii].end_pts) {
                GC_ERROR("_render_textst: overlapping dialogs detected\n");
            }
        }

        /* push hide events */
        for (jj = 0; jj < dialog[ii].region_count; jj++) {
          BD_TEXTST_DIALOG_REGION *region = &dialog[ii].region[jj];
          BD_TEXTST_REGION_STYLE  *style = NULL;

          style = _find_region_style(s->style, region->region_style_id_ref);
          if (!style) {
            continue;
          }
          _clear_osd_area(p, BD_OVERLAY_PG, dialog[ii].end_pts,
                          style->region_info.region.xpos, style->region_info.region.ypos,
                          style->region_info.region.width, style->region_info.region.height);
        }

        _hide_osd(p, BD_OVERLAY_PG);

        /* commit changes */
        _flush_osd(p, BD_OVERLAY_PG, dialog[ii].end_pts);
    }

    return 0;
}


/*
 * PG rendering
 */

static int _render_pg_composition_object(GRAPHICS_CONTROLLER *gc,
                                         int64_t pts,
                                         BD_PG_COMPOSITION_OBJECT *cobj,
                                         BD_PG_PALETTE *palette)
{
    BD_PG_COMPOSITION *pcs     = gc->pgs->pcs;
    BD_PG_OBJECT      *object  = NULL;

    /* lookup object */
    object  = _find_object(gc->pgs, cobj->object_id_ref);
    if (!object) {
        GC_ERROR("_render_pg_composition_object: object #%d not found\n", cobj->object_id_ref);
        return -1;
    }

    /* open PG overlay */
    if (!gc->pg_open) {
        _open_osd(gc, BD_OVERLAY_PG, 0, 0, pcs->video_descriptor.video_width, pcs->video_descriptor.video_height);
    }

    /* render object using composition parameters */
    _render_composition_object(gc, pts, BD_OVERLAY_PG, cobj, object, palette, pcs->palette_update_flag);

    return 0;
}

static int _render_pg(GRAPHICS_CONTROLLER *gc)
{
    PG_DISPLAY_SET    *s       = gc->pgs;
    BD_PG_COMPOSITION *pcs     = NULL;
    BD_PG_PALETTE     *palette = NULL;
    unsigned           display_flag;
    unsigned           ii;

    if (!s || !s->pcs || !s->complete) {
        GC_ERROR("_render_pg(): no composition\n");
        return -1;
    }
    pcs = s->pcs;

    /* mark PG display set handled */
    gc->pgs->complete = 0;

    /* lookup palette */
    palette = _find_palette(gc->pgs, pcs->palette_id_ref);
    if (!palette) {
        GC_ERROR("_render_pg(): unknown palette id %d (have %d palettes)\n",
                 pcs->palette_id_ref, s->num_palette);
        return -1;
    }

    display_flag = bd_psr_read(gc->regs, PSR_PG_STREAM) >> 31;

    /* render objects */
    for (ii = 0; ii < pcs->num_composition_objects; ii++) {
        BD_PG_COMPOSITION_OBJECT *cobj = &pcs->composition_object[ii];
        if (cobj->forced_on_flag) {
            GC_ERROR("_render_pg(): forced_on_flag not implemented\n");
        }
        if (cobj->forced_on_flag || display_flag) {
            _render_pg_composition_object(gc, pcs->pts, cobj, palette);
        }
    }

    if (!gc->pg_open) {
        return 0;
    }

    /* commit changes at given pts */
    _flush_osd(gc, BD_OVERLAY_PG, pcs->pts);

    /* clear plane but do not commit changes yet */
    /* -> plane will be cleared and hidden when empty composition arrives */
    /* (-> no need to store object regions for next update / clear event - or use expensive full plane clear) */
    for (ii = 0; ii < pcs->num_composition_objects; ii++) {
        BD_PG_COMPOSITION_OBJECT *cobj   = &pcs->composition_object[ii];
        BD_PG_OBJECT             *object = _find_object(gc->pgs, cobj->object_id_ref);

        if (object) {
            _clear_osd_area(gc, BD_OVERLAY_PG, -1,
                            cobj->x, cobj->y, object->width, object->height);
        }
    }
    _hide_osd(gc, BD_OVERLAY_PG);

    return 0;
}

static void _reset_pg(GRAPHICS_CONTROLLER *gc)
{
    graphics_processor_free(&gc->pgp);

    pg_display_set_free(&gc->pgs);

    if (gc->pg_open) {
        _close_osd(gc, BD_OVERLAY_PG);
    }

    gc->next_dialog_idx = 0;
}

/*
 * IG rendering
 */

static void _render_button(GRAPHICS_CONTROLLER *gc, BD_IG_BUTTON *button, BD_PG_PALETTE *palette,
                           int state, BOG_DATA *bog_data)
{
    BD_PG_OBJECT *object = _find_object_for_button(gc->igs, button, state, bog_data);
    if (!object) {
        GC_TRACE("_render_button(#%d): object (state %d) not found\n", button->id, state);

        _clear_bog_area(gc, bog_data);

        return;
    }

    /* object already rendered ? */
    if (bog_data->visible_object_id == object->id &&
        bog_data->x == button->x_pos && bog_data->y == button->y_pos &&
        bog_data->w == object->width && bog_data->h == object->height) {

        GC_TRACE("skipping already rendered button #%d (object #%d at %d,%d %dx%d)\n",
                 button->id, object->id,
                 button->x_pos, button->y_pos, object->width, object->height);

        return;
    }

    /* new object is smaller than already drawn one, or in different position ? -> need to render background */
    if (bog_data->w > object->width ||
        bog_data->h > object->height ||
        bog_data->x != button->x_pos ||
        bog_data->y != button->y_pos) {

        /* make sure we won't wipe other buttons */
        unsigned ii, skip = 0;
        for (ii = 0; &gc->bog_data[ii] != bog_data; ii++) {
            if (_areas_overlap(bog_data, &gc->bog_data[ii]))
                skip = 1;
            /* FIXME: clean non-overlapping area */
        }

        GC_TRACE("object size changed, %sclearing background at %d,%d %dx%d\n",
                 skip ? " ** NOT ** " : "",
                 bog_data->x, bog_data->y, bog_data->w, bog_data->h);

        if (!skip) {
            _clear_bog_area(gc, bog_data);
        }
    }

    GC_TRACE("render button #%d using object #%d at %d,%d %dx%d\n",
             button->id, object->id,
             button->x_pos, button->y_pos, object->width, object->height);

    _render_object(gc, -1, BD_OVERLAY_IG,
                   button->x_pos, button->y_pos,
                   object, palette);

    bog_data->x = button->x_pos;
    bog_data->y = button->y_pos;
    bog_data->w = object->width;
    bog_data->h = object->height;
    bog_data->visible_object_id = object->id;

    gc->ig_drawn = 1;
    gc->ig_dirty = 1;
}

static int _render_ig_composition_object(GRAPHICS_CONTROLLER *gc,
                                         int64_t pts,
                                         BD_PG_COMPOSITION_OBJECT *cobj,
                                         BD_PG_PALETTE *palette)
{
    BD_PG_OBJECT   *object  = NULL;

    /* lookup object */
    object  = _find_object(gc->igs, cobj->object_id_ref);
    if (!object) {
        GC_ERROR("_render_ig_composition_object: object #%d not found\n", cobj->object_id_ref);
        return -1;
    }

    /* render object using composition parameters */
    _render_composition_object(gc, pts, BD_OVERLAY_IG, cobj, object, palette, 0);

    return 0;
}

static int _render_effect(GRAPHICS_CONTROLLER *gc, BD_IG_EFFECT *effect)
{
    BD_PG_PALETTE *palette = NULL;
    unsigned ii;
    int64_t pts = -1;

    if (!gc->ig_open) {
        _open_osd(gc, BD_OVERLAY_IG, 0, 0,
                  gc->igs->ics->video_descriptor.video_width,
                  gc->igs->ics->video_descriptor.video_height);
    }

    _clear_osd(gc, BD_OVERLAY_IG);

    palette = _find_palette(gc->igs, effect->palette_id_ref);
    if (!palette) {
        GC_ERROR("_render_effect: palette #%d not found\n", effect->palette_id_ref);
        return -1;
    }

    for (ii = 0; ii < effect->num_composition_objects; ii++) {
        _render_ig_composition_object(gc, pts, &effect->composition_object[ii], palette);
    }

    _flush_osd(gc, BD_OVERLAY_IG, pts);

    _reset_user_timeout(gc);

    return 0;
}

static int _render_page(GRAPHICS_CONTROLLER *gc,
                         unsigned activated_button_id,
                         GC_NAV_CMDS *cmds)
{
    PG_DISPLAY_SET *s       = gc->igs;
    BD_IG_PAGE     *page    = NULL;
    BD_PG_PALETTE  *palette = NULL;
    unsigned        page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        ii;
    unsigned        selected_button_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);
    BD_IG_BUTTON   *auto_activate_button = NULL;

    gc->button_effect_running = 0;
    gc->button_animation_running = 0;

    if (s->ics->interactive_composition.ui_model == IG_UI_MODEL_POPUP && !gc->popup_visible) {

        gc->page_uo_mask = bd_empty_uo_mask();

        if (gc->ig_open) {
            GC_TRACE("_render_page(): popup menu not visible\n");
            _close_osd(gc, BD_OVERLAY_IG);
            return 1;
        }

        return 0;
    }

    /* running page effects ? */
    if (gc->out_effects) {
        if (gc->effect_idx < gc->out_effects->num_effects) {
            _render_effect(gc, &gc->out_effects->effect[gc->effect_idx]);
            return 1;
        }
        gc->out_effects = NULL;
    }

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        GC_ERROR("_render_page: unknown page id %d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return -1;
    }

    gc->page_uo_mask = page->uo_mask_table;

    if (gc->in_effects) {
        if (gc->effect_idx < gc->in_effects->num_effects) {
            _render_effect(gc, &gc->in_effects->effect[gc->effect_idx]);
            return 1;
        }
        gc->in_effects = NULL;
    }

    palette = _find_palette(s, page->palette_id_ref);
    if (!palette) {
        GC_ERROR("_render_page: unknown palette id %d (have %d palettes)\n",
              page->palette_id_ref, s->num_palette);
        return -1;
    }

    GC_TRACE("rendering page #%d using palette #%d. page has %d bogs\n",
          page->id, page->palette_id_ref, page->num_bogs);

    if (!gc->ig_open) {
        _open_osd(gc, BD_OVERLAY_IG, 0, 0,
                  s->ics->video_descriptor.video_width,
                  s->ics->video_descriptor.video_height);
    }

    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BOG    *bog      = &page->bog[ii];
        unsigned      valid_id = gc->bog_data[ii].enabled_button;
        BD_IG_BUTTON *button;

        button = _find_button_bog(bog, valid_id);

        if (!button) {
            GC_TRACE("_render_page(): bog %d: button %d not found\n", ii, valid_id);

            // render background
            _clear_bog_area(gc, &gc->bog_data[ii]);

        } else if (button->id == activated_button_id) {
            GC_TRACE("    button #%d activated\n", button->id);

            _render_button(gc, button, palette, BTN_ACTIVATED, &gc->bog_data[ii]);

        } else if (button->id == selected_button_id) {

            if (button->auto_action_flag && !gc->auto_action_triggered) {
                if (cmds) {
                    if (!auto_activate_button) {
                        auto_activate_button = button;
                    }
                } else {
                    GC_ERROR("   auto-activate #%d not triggered (!cmds)\n", button->id);
                }

                _render_button(gc, button, palette, BTN_ACTIVATED, &gc->bog_data[ii]);

            } else {
                _render_button(gc, button, palette, BTN_SELECTED, &gc->bog_data[ii]);
            }

        } else {
            _render_button(gc, button, palette, BTN_NORMAL, &gc->bog_data[ii]);

        }

        gc->button_effect_running    += gc->bog_data[ii].effect_running;
        gc->button_animation_running += (gc->bog_data[ii].animate_indx >= 0);
    }

    /* process auto-activate */
    if (auto_activate_button) {
        GC_TRACE("   auto-activate #%d\n", auto_activate_button->id);

        /* do not trigger auto action before single-loop animations have been terminated */
        if (gc->button_effect_running) {
            GC_TRACE("   auto-activate #%d not triggered (ANIMATING)\n", auto_activate_button->id);
        } else if (cmds) {
            cmds->num_nav_cmds = auto_activate_button->num_nav_cmds;
            cmds->nav_cmds     = auto_activate_button->nav_cmds;

            gc->auto_action_triggered = 1;
        } else {
            GC_ERROR("_render_page(): auto-activate ignored (missing result buffer)\n");
        }
    }

    if (gc->ig_dirty) {
        _flush_osd(gc, BD_OVERLAY_IG, -1);
        gc->ig_dirty = 0;
        return 1;
    }

    return 0;
}

/*
 * user actions
 */

#define VK_IS_NUMERIC(vk) (/*vk >= BD_VK_0  &&*/ vk <= BD_VK_9)
#define VK_IS_CURSOR(vk)  (vk >= BD_VK_UP && vk <= BD_VK_RIGHT)
#define VK_TO_NUMBER(vk)  ((vk) - BD_VK_0)

static int _user_input(GRAPHICS_CONTROLLER *gc, uint32_t key, GC_NAV_CMDS *cmds)
{
    PG_DISPLAY_SET *s          = gc->igs;
    BD_IG_PAGE     *page       = NULL;
    unsigned        page_id    = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        cur_btn_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);
    unsigned        new_btn_id = cur_btn_id;
    unsigned        ii;
    int             activated_btn_id = -1;

    if (s->ics->interactive_composition.ui_model == IG_UI_MODEL_POPUP && !gc->popup_visible) {
        GC_TRACE("_user_input(): popup menu not visible\n");
        return -1;
    }
    if (!gc->ig_open) {
        GC_ERROR("_user_input(): menu not open\n");
        return -1;
    }

    if (!gc->ig_drawn) {
        GC_ERROR("_user_input(): menu not visible\n");
        return 0;
    }

    _reset_user_timeout(gc);

    if (gc->button_effect_running) {
        GC_ERROR("_user_input(): button_effect_running\n");
        return 0;
    }

    GC_TRACE("_user_input(%d)\n", key);

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        GC_ERROR("_user_input(): unknown page id %d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return -1;
    }

    if (key == BD_VK_MOUSE_ACTIVATE) {
        if (!gc->valid_mouse_position) {
            GC_TRACE("_user_input(): BD_VK_MOUSE_ACTIVATE outside of valid buttons\n");
            return -1;
        }
        key = BD_VK_ENTER;
    }

    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BOG *bog      = &page->bog[ii];
        unsigned   valid_id = gc->bog_data[ii].enabled_button;
        BD_IG_BUTTON *button = _find_button_bog(bog, valid_id);
        if (!button) {
            continue;
        }

        /* numeric select */
        if (VK_IS_NUMERIC(key)) {
            if (button->numeric_select_value == VK_TO_NUMBER(key)) {
                new_btn_id = button->id;
            }
        }

        /* cursor keys */
        else if (VK_IS_CURSOR(key) || key == BD_VK_ENTER) {
            if (button->id == cur_btn_id) {
                switch(key) {
                    case BD_VK_UP:
                        new_btn_id = button->upper_button_id_ref;
                        break;
                    case BD_VK_DOWN:
                        new_btn_id = button->lower_button_id_ref;
                        break;
                    case BD_VK_LEFT:
                        new_btn_id = button->left_button_id_ref;
                        break;
                    case BD_VK_RIGHT:
                        new_btn_id = button->right_button_id_ref;
                        break;
                    case BD_VK_ENTER:
                        activated_btn_id = cur_btn_id;

                        if (cmds) {
                            cmds->num_nav_cmds = button->num_nav_cmds;
                            cmds->nav_cmds     = button->nav_cmds;
                            cmds->sound_id_ref = button->activated_sound_id_ref;
                        } else {
                            GC_ERROR("_user_input(): VD_VK_ENTER action ignored (missing result buffer)\n");
                        }
                        break;
                    default:;
                }
            }

            if (new_btn_id != cur_btn_id) {
                BD_IG_BUTTON *new_button = _find_button_page(page, new_btn_id, NULL);
                if (new_button && cmds) {
                    cmds->sound_id_ref = new_button->selected_sound_id_ref;
                }
            }
        }
    }

    /* render page ? */
    if (new_btn_id != cur_btn_id || activated_btn_id >= 0) {

        _select_button(gc, new_btn_id);

        _render_page(gc, activated_btn_id, cmds);

        /* found one*/
        return 1;
    }

    return 0;
}

static void _set_button_page(GRAPHICS_CONTROLLER *gc, uint32_t param)
{
    unsigned page_flag   = param & 0x80000000;
    unsigned effect_flag = param & 0x40000000;
    unsigned button_flag = param & 0x20000000;
    unsigned page_id     = (param >> 16) & 0xff;
    unsigned button_id   = param & 0xffff;
    unsigned bog_idx     = 0;

    PG_DISPLAY_SET *s      = gc->igs;
    BD_IG_PAGE     *page   = NULL;
    BD_IG_BUTTON   *button = NULL;

    GC_TRACE("_set_button_page(0x%08x): page flag %d, id %d, effects %d   button flag %d, id %d\n",
          param, !!page_flag, page_id, !!effect_flag, !!button_flag, button_id);

    /* 10.4.3.4 (D) */

    if (!page_flag && !button_flag) {
        return;
    }

    if (page_flag) {

        /* current page --> command is ignored */
        if (page_id == bd_psr_read(gc->regs, PSR_MENU_PAGE_ID)) {
            GC_TRACE("  page is current\n");
            return;
        }

        page = _find_page(&s->ics->interactive_composition, page_id);

        /* invalid page --> command is ignored */
        if (!page) {
            GC_TRACE("  page is invalid\n");
            return;
        }

        /* page changes */

        _select_page(gc, page_id, !effect_flag);

    } else {
        /* page does not change */
        page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
        page    = _find_page(&s->ics->interactive_composition, page_id);

        if (!page) {
            GC_ERROR("_set_button_page(): PSR_MENU_PAGE_ID refers to unknown page %d\n", page_id);
            return;
        }
    }

    if (button_flag) {
        /* find correct button and overlap group */
        button = _find_button_page(page, button_id, &bog_idx);

        if (!page_flag) {
            if (!button) {
                /* page not given, invalid button --> ignore command */
                GC_TRACE("  button is invalid\n");
                return;
            }
            if (button_id == bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID)) {
                /* page not given, current button --> ignore command */
                GC_TRACE("  button is current\n");
                return;
            }
        }
    }

    if (button) {
        gc->bog_data[bog_idx].enabled_button = button_id;
        _select_button(gc, button_id);
    }

    _render_page(gc, 0xffff, NULL); /* auto action not triggered yet */
}

static void _enable_button(GRAPHICS_CONTROLLER *gc, uint32_t button_id, unsigned enable)
{
    PG_DISPLAY_SET *s          = gc->igs;
    BD_IG_PAGE     *page       = NULL;
    BD_IG_BUTTON   *button     = NULL;
    unsigned        page_id    = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        cur_btn_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);
    unsigned        bog_idx    = 0;

    GC_TRACE("_enable_button(#%d, %s)\n", button_id, enable ? "enable" : "disable");

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        GC_TRACE("_enable_button(): unknown page #%d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return;
    }

    /* find correct button overlap group */
    button = _find_button_page(page, button_id, &bog_idx);
    if (!button) {
        GC_TRACE("_enable_button(): unknown button #%d (page #%d)\n", button_id, page_id);
        return;
    }

    if (enable) {
        if (gc->bog_data[bog_idx].enabled_button == cur_btn_id) {
            /* selected button goes to disabled state */
            bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, 0x10000|button_id);
        }
        gc->bog_data[bog_idx].enabled_button = button_id;
        gc->bog_data[bog_idx].animate_indx = 0;

    } else {
        if (gc->bog_data[bog_idx].enabled_button == button_id) {
            gc->bog_data[bog_idx].enabled_button = 0xffff;
        }

        if (cur_btn_id == button_id) {
            bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, 0xffff);
        }
    }
}

static void _update_selected_button(GRAPHICS_CONTROLLER *gc)
{
    /* executed after IG command sequence terminates */
    unsigned button_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);

    GC_TRACE("_update_selected_button(): currently enabled button is #%d\n", button_id);

    /* special case: triggered only after enable button disables selected button */
    if (button_id & 0x10000) {
        button_id &= 0xffff;
        _select_button(gc, button_id);
        GC_TRACE("_update_selected_button() -> #%d [last enabled]\n", button_id);
        return;
    }

    if (button_id == 0xffff) {
        button_id = _find_selected_button_id(gc);
        _select_button(gc, button_id);
    }
}

static int _mouse_move(GRAPHICS_CONTROLLER *gc, uint16_t x, uint16_t y, GC_NAV_CMDS *cmds)
{
    PG_DISPLAY_SET *s          = gc->igs;
    BD_IG_PAGE     *page       = NULL;
    unsigned        page_id    = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        cur_btn_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);
    unsigned        new_btn_id = 0xffff;
    unsigned        ii;

    gc->valid_mouse_position = 0;

    if (!gc->ig_drawn) {
        GC_TRACE("_mouse_move(): menu not visible\n");
        return -1;
    }

    if (gc->button_effect_running) {
        GC_ERROR("_mouse_move(): button_effect_running\n");
        return -1;
    }

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        GC_ERROR("_mouse_move(): unknown page #%d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return -1;
    }

    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BOG    *bog      = &page->bog[ii];
        unsigned      valid_id = gc->bog_data[ii].enabled_button;
        BD_IG_BUTTON *button   = _find_button_bog(bog, valid_id);

        if (!button)
            continue;

        if (x < button->x_pos || y < button->y_pos)
            continue;

        /* Check for SELECTED state object (button that can be selected) */
        BD_PG_OBJECT *object = _find_object_for_button(s, button, BTN_SELECTED, NULL);
        if (!object)
            continue;

        if (x >= button->x_pos + object->width || y >= button->y_pos + object->height)
            continue;

        /* mouse is over button */
        gc->valid_mouse_position = 1;

        /* is button already selected? */
        if (button->id == cur_btn_id) {
            return 1;
        }

        new_btn_id = button->id;

        if (cmds) {
            cmds->sound_id_ref = button->selected_sound_id_ref;
        }

        break;
    }

    if (new_btn_id != 0xffff) {
        _select_button(gc, new_btn_id);

        _render_page(gc, -1, cmds);

        _reset_user_timeout(gc);
    }

    return gc->valid_mouse_position;
}

static int _animate(GRAPHICS_CONTROLLER *gc, GC_NAV_CMDS *cmds)
{
    int result = -1;

    if (gc->ig_open) {

        result = 0;

        if (gc->out_effects) {
            int64_t pts = bd_get_scr();
            int64_t duration = (int64_t)gc->out_effects->effect[gc->effect_idx].duration;
            if (pts >= (gc->next_effect_time + duration)) {
                gc->next_effect_time += duration;
                gc->effect_idx++;
                if (gc->effect_idx >= gc->out_effects->num_effects) {
                    gc->out_effects = NULL;
                    gc->effect_idx = 0;
                    _clear_osd(gc, BD_OVERLAY_IG);
                }
                result = _render_page(gc, 0xffff, cmds);
            }
        } else if (gc->in_effects) {
            int64_t pts = bd_get_scr();
            int64_t duration = (int64_t)gc->in_effects->effect[gc->effect_idx].duration;
            if (pts >= (gc->next_effect_time + duration)) {
                gc->next_effect_time += duration;
                gc->effect_idx++;
                if (gc->effect_idx >= gc->in_effects->num_effects) {
                    gc->in_effects = NULL;
                    gc->effect_idx = 0;
                    _clear_osd(gc, BD_OVERLAY_IG);
                }
                result = _render_page(gc, 0xffff, cmds);
            }

        } else if (gc->button_animation_running) {
            int64_t pts = bd_get_scr();
            if (pts >= (gc->next_effect_time + gc->frame_interval)) {
                gc->next_effect_time += gc->frame_interval;
                result = _render_page(gc, 0xffff, cmds);
            }
        }
    }

    return result;
}

static int _run_timers(GRAPHICS_CONTROLLER *gc, GC_NAV_CMDS *cmds)
{
    int result = -1;

    if (gc->ig_open) {

        result = 0;

        if (gc->user_timeout) {
            int64_t pts = bd_get_scr();
            if (pts > gc->user_timeout) {

                GC_TRACE("user timeout expired\n");

                if (gc->igs->ics->interactive_composition.ui_model != IG_UI_MODEL_POPUP) {

                    if (bd_psr_read(gc->regs, PSR_MENU_PAGE_ID) != 0) {
                        _select_page(gc, 0, 0);
                        result = _render_page(gc, 0xffff, cmds);
                    }

                } else {
                    gc->popup_visible = 0;
                    result = _render_page(gc, 0xffff, cmds);
                }
            }
        }
    }

    return result;
}

int gc_run(GRAPHICS_CONTROLLER *gc, gc_ctrl_e ctrl, uint32_t param, GC_NAV_CMDS *cmds)
{
    int result = -1;

    if (cmds) {
        cmds->num_nav_cmds = 0;
        cmds->nav_cmds     = NULL;
        cmds->sound_id_ref = -1;
        cmds->status       = GC_STATUS_NONE;
        cmds->page_uo_mask = bd_empty_uo_mask();
    }

    if (!gc) {
        GC_TRACE("gc_run(): no graphics controller\n");
        return result;
    }

    bd_mutex_lock(&gc->mutex);

    /* always accept reset */
    switch (ctrl) {
        case GC_CTRL_RESET:
            _gc_reset(gc);

            bd_mutex_unlock(&gc->mutex);
            return 0;
        case GC_CTRL_PG_UPDATE:
            if (gc->pgs && gc->pgs->pcs) {
                result = _render_pg(gc);
            }
            if (gc->tgs && gc->tgs->dialog) {
                result = _render_textst(gc, param, cmds);
            }
            bd_mutex_unlock(&gc->mutex);
            return result;

        case GC_CTRL_STYLE_SELECT:
            result = _textst_style_select(gc, param);
            bd_mutex_unlock(&gc->mutex);
            return result;

        case GC_CTRL_PG_CHARCODE:
            if (gc->textst_render) {
                textst_render_set_char_code(gc->textst_render, param);
                result = 0;
            }
            bd_mutex_unlock(&gc->mutex);
            return result;

        case GC_CTRL_PG_RESET:
            _reset_pg(gc);

            bd_mutex_unlock(&gc->mutex);
            return 0;

        default:;
    }

    /* other operations require complete display set */
    if (!gc->igs || !gc->igs->ics || !gc->igs->complete) {
        GC_TRACE("gc_run(): no interactive composition\n");
        bd_mutex_unlock(&gc->mutex);
        return result;
    }

    switch (ctrl) {

        case GC_CTRL_SET_BUTTON_PAGE:
            _set_button_page(gc, param);
            break;

        case GC_CTRL_VK_KEY:
            if (param != BD_VK_POPUP) {
                result = _user_input(gc, param, cmds);
                break;
            }
            param = !gc->popup_visible;
            /* fall thru (BD_VK_POPUP) */

        case GC_CTRL_POPUP:
            if (gc->igs->ics->interactive_composition.ui_model != IG_UI_MODEL_POPUP) {
                /* not pop-up menu */
                break;
            }

            gc->popup_visible = !!param;

            if (gc->popup_visible) {
                _select_page(gc, 0, 0);
            }

            result = _render_page(gc, 0xffff, cmds);
            break;

        case GC_CTRL_NOP:
            result = _animate(gc, cmds);
            _run_timers(gc, cmds);
            break;

        case GC_CTRL_INIT_MENU:
            _select_page(gc, 0, 0);
            _render_page(gc, 0xffff, cmds);
            break;

        case GC_CTRL_IG_END:
            _update_selected_button(gc);
            _render_page(gc, 0xffff, cmds);
            break;

        case GC_CTRL_ENABLE_BUTTON:
            _enable_button(gc, param, 1);
            break;

        case GC_CTRL_DISABLE_BUTTON:
            _enable_button(gc, param, 0);
            break;

        case GC_CTRL_MOUSE_MOVE:
            result = _mouse_move(gc, param >> 16, param & 0xffff, cmds);
            break;

        case GC_CTRL_RESET:
        case GC_CTRL_PG_RESET:
        case GC_CTRL_PG_UPDATE:
        case GC_CTRL_PG_CHARCODE:
        case GC_CTRL_STYLE_SELECT:
            /* already handled */
            break;
    }

    if (cmds) {
        if (gc->igs->ics->interactive_composition.ui_model == IG_UI_MODEL_POPUP) {
            cmds->status |= GC_STATUS_POPUP;
        }
        if (gc->ig_drawn) {
            cmds->status |= GC_STATUS_MENU_OPEN;
        }
        if (gc->in_effects || gc->out_effects || gc->button_animation_running || gc->user_timeout) {
            /* do not trigger if unopened pop-up menu has animations */
            if (gc->ig_open) {
                cmds->status |= GC_STATUS_ANIMATE;
                /* user input is still not handled, but user "sees" the menu. */
                cmds->status |= GC_STATUS_MENU_OPEN;
            }
        }

        if (gc->ig_open) {
            cmds->page_uo_mask = gc->page_uo_mask;
        }
    }

    bd_mutex_unlock(&gc->mutex);

    return result;
}
