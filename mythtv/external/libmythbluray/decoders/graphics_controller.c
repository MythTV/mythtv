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

#include "graphics_controller.h"

#include "graphics_processor.h"
#include "ig.h"
#include "overlay.h"

#include "util/macro.h"
#include "util/logging.h"
#include "util/mutex.h"

#include "../register.h"
#include "../keys.h"

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

    /* data */
    PG_DISPLAY_SET *pgs;
    PG_DISPLAY_SET *igs;

    /* */
    GRAPHICS_PROCESSOR *pgp;
    GRAPHICS_PROCESSOR *igp;
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
        if (bog_data->animate_indx >= 0) {
            int range = object_id_end - object_id;

            if (range > 0 && object_id < 0xffff && object_id_end < 0xffff) {
                GC_TRACE("animate button #%d: animate_indx %d, range %d, repeat %d\n",
                         button->id, bog_data->animate_indx, range, repeat);

                object_id += bog_data->animate_indx % (range + 1);
                bog_data->animate_indx++;
                if (!repeat && bog_data->animate_indx > range) {
                /* terminate animation to the last object */
                    bog_data->animate_indx = -1;
                }

            } else {
                /* no animation for this button */
                bog_data->animate_indx = -1;
            }
        }
    }

    if (!repeat && object_id_end < 0xfffe) {
        object_id = object_id_end;
    }

    object = _find_object(s, object_id);

    return object;
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

static int _save_page_state(GRAPHICS_CONTROLLER *gc)
{
    if (!gc->bog_data) {
        GC_ERROR("_save_page_state(): no bog data !\n");
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

    for (ii = 0; ii < page->num_bogs; ii++) {
        gc->saved_bog_data[ii].enabled_button = gc->bog_data[ii].enabled_button;
        gc->saved_bog_data[ii].animate_indx   = gc->bog_data[ii].animate_indx >= 0 ? 0 : -1;
    }

    return 1;
}

static int _restore_page_state(GRAPHICS_CONTROLLER *gc)
{
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
}

/*
 * overlay operations
 */

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static void _open_osd(GRAPHICS_CONTROLLER *gc, int plane,
                      unsigned width, unsigned height)
{
    if (gc->overlay_proc) {
        BD_OVERLAY ov = {0};
        ov.cmd          = BD_OVERLAY_INIT;
        ov.pts          = -1;
        ov.plane        = plane;
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

static void _clear_osd_area(GRAPHICS_CONTROLLER *gc, int plane,
                            uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (gc->overlay_proc) {
        /* wipe area */
        BD_OVERLAY ov = {0};
        ov.cmd     = BD_OVERLAY_WIPE;
        ov.pts     = -1;
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

        _clear_osd_area(gc, BD_OVERLAY_IG, bog_data->x, bog_data->y, bog_data->w, bog_data->h);

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

/*
 * page selection and IG effects
 */

static void _select_button(GRAPHICS_CONTROLLER *gc, uint32_t button_id)
{
    bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, button_id);
    gc->auto_action_triggered = 0;
}

static void _select_page(GRAPHICS_CONTROLLER *gc, uint16_t page_id)
{
    bd_psr_write(gc->regs, PSR_MENU_PAGE_ID, page_id);
    if (gc->ig_open) {
        _clear_osd(gc, BD_OVERLAY_IG);
    }
    _reset_page_state(gc);

    uint16_t button_id = _find_selected_button_id(gc);
    _select_button(gc, button_id);

    gc->valid_mouse_position = 0;
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

    graphics_processor_free(&gc->igp);
    graphics_processor_free(&gc->pgp);

    pg_display_set_free(&gc->pgs);
    pg_display_set_free(&gc->igs);

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

    p->regs = regs;

    p->overlay_proc_handle = handle;
    p->overlay_proc        = func;

    bd_mutex_init(&p->mutex);

    bd_psr_register_cb(regs, _process_psr_event, p);

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

    if (pid >= 0x1400 && pid < 0x1500) {
        /* IG stream */

        if (!gc->igp) {
            gc->igp = graphics_processor_init();
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

        bd_mutex_unlock(&gc->mutex);

        return 1;
    }

    else if (pid >= 0x1200 && pid < 0x1300) {
        /* PG stream */
        if (!gc->pgp) {
            gc->pgp = graphics_processor_init();
        }
        graphics_processor_decode_ts(gc->pgp, &gc->pgs,
                                     pid, block, num_blocks,
                                     stc);

        if (!gc->pgs || !gc->pgs->complete) {
            return 0;
        }

        return 1;
    }

    return -1;
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

    /* new object is smaller than already drawn one ? -> need to render background */
    if (bog_data->w > object->width ||
        bog_data->h > object->height) {

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

    if (s->ics->interactive_composition.ui_model == IG_UI_MODEL_POPUP && !gc->popup_visible) {

        if (gc->ig_open) {
            GC_TRACE("_render_page(): popup menu not visible\n");
            _close_osd(gc, BD_OVERLAY_IG);
            return 1;
        }

        return 0;
    }

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        GC_ERROR("_render_page: unknown page id %d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return -1;
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
        _open_osd(gc, BD_OVERLAY_IG,
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
                    GC_TRACE("   auto-activate #%d\n", button->id);

                    cmds->num_nav_cmds = button->num_nav_cmds;
                    cmds->nav_cmds     = button->nav_cmds;

                    gc->auto_action_triggered = 1;
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
    if (!gc->ig_drawn) {
        GC_ERROR("_user_input(): menu not visible\n");
        return -1;
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

                        cmds->num_nav_cmds = button->num_nav_cmds;
                        cmds->nav_cmds     = button->nav_cmds;
                        cmds->sound_id_ref = button->activated_sound_id_ref;
                        break;
                    default:;
                }
            }

            if (new_btn_id != cur_btn_id) {
                BD_IG_BUTTON *new_button = _find_button_page(page, new_btn_id, NULL);
                if (new_button) {
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

        _select_page(gc, page_id);

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

static int _mouse_move(GRAPHICS_CONTROLLER *gc, unsigned x, unsigned y, GC_NAV_CMDS *cmds)
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
    }

    return gc->valid_mouse_position;
}

int gc_run(GRAPHICS_CONTROLLER *gc, gc_ctrl_e ctrl, uint32_t param, GC_NAV_CMDS *cmds)
{
    int result = -1;

    if (cmds) {
        cmds->num_nav_cmds = 0;
        cmds->nav_cmds     = NULL;
        cmds->sound_id_ref = -1;
        cmds->status       = GC_STATUS_NONE;
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
                _select_page(gc, 0);
            }

            /* fall thru */

        case GC_CTRL_NOP:
            result = _render_page(gc, 0xffff, cmds);
            break;

        case GC_CTRL_INIT_MENU:
            _select_page(gc, 0);
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
    }

    bd_mutex_unlock(&gc->mutex);

    return result;
}
