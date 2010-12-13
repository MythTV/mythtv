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

#include "../register.h"
#include "../keys.h"

#include <inttypes.h>

#define ERROR(x,...) DEBUG(DBG_BLURAY | DBG_CRIT, x, ##__VA_ARGS__)
#define TRACE(x,...) DEBUG(DBG_BLURAY,            x, ##__VA_ARGS__)

/*
 *
 */

struct graphics_controller_s {

    BD_REGISTERS   *regs;

    /* overlay output */
    void           *overlay_proc_handle;
    void          (*overlay_proc)(void *, const struct bd_overlay_s * const);

    /* state */
    unsigned        ig_drawn;
    unsigned        pg_drawn;
    unsigned        popup_visible;
    unsigned        valid_mouse_position;

    /* data */
    PG_DISPLAY_SET *pgs;
    PG_DISPLAY_SET *igs;
    uint16_t       *enabled_button;

    /* */
    GRAPHICS_PROCESSOR *pgp;
    GRAPHICS_PROCESSOR *igp;
};

GRAPHICS_CONTROLLER *gc_init(BD_REGISTERS *regs, void *handle, gc_overlay_proc_f func)
{
    GRAPHICS_CONTROLLER *p = calloc(1, sizeof(*p));

    p->regs = regs;

    p->overlay_proc_handle = handle;
    p->overlay_proc        = func;

    return p;
}

static void _gc_clear_osd(GRAPHICS_CONTROLLER *gc, int plane)
{
    if (gc->overlay_proc) {
        /* clear plane */
        const BD_OVERLAY ov = {
            .pts     = -1,
            .plane   = plane,
            .x       = 0,
            .y       = 0,
            .w       = 1920,
            .h       = 1080,
            .palette = NULL,
            .img     = NULL,
        };

        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }

    if (plane) {
        gc->ig_drawn      = 0;
    } else {
        gc->pg_drawn      = 0;
    }
}

static void _gc_reset(GRAPHICS_CONTROLLER *gc)
{
    _gc_clear_osd(gc, 0);
    _gc_clear_osd(gc, 1);

    gc->popup_visible = 0;

    graphics_processor_free(&gc->igp);
    graphics_processor_free(&gc->pgp);

    pg_display_set_free(&gc->pgs);
    pg_display_set_free(&gc->igs);

    X_FREE(gc->enabled_button);
}

void gc_free(GRAPHICS_CONTROLLER **p)
{
    if (p && *p) {

        _gc_reset(*p);

        if ((*p)->overlay_proc) {
            (*p)->overlay_proc((*p)->overlay_proc_handle, NULL);
        }

        X_FREE(*p);
    }
}

/*
 * graphics stream input
 */

static void _reset_enabled_button(GRAPHICS_CONTROLLER *gc);

void gc_decode_ts(GRAPHICS_CONTROLLER *gc, uint16_t pid, uint8_t *block, unsigned num_blocks, int64_t stc)
{
    if (pid >= 0x1400 && pid < 0x1500) {
        /* IG stream */

        if (!gc->igp) {
            gc->igp = graphics_processor_init();
        }
        graphics_processor_decode_ts(gc->igp, &gc->igs,
                                     pid, block, num_blocks,
                                     stc);
        if (!gc->igs || !gc->igs->complete) {
            return;
        }

        bd_psr_write(gc->regs, PSR_MENU_PAGE_ID, 0);

        gc->ig_drawn = 0;
        gc->popup_visible = 0;

        _gc_clear_osd(gc, 1);
        _reset_enabled_button(gc);
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
            return;
        }
    }
}

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
                                             BD_IG_BUTTON *button, int state)
{
    BD_PG_OBJECT *object   = NULL;
    unsigned object_id     = 0xffff;

    switch (state) {
        case BTN_NORMAL:
            object_id = button->normal_start_object_id_ref;
            break;
        case BTN_SELECTED:
            object_id = button->selected_start_object_id_ref;
            break;
        case BTN_ACTIVATED:
            object_id = button->activated_start_object_id_ref;
            break;
    }

    object = _find_object(s, object_id);

    return object;
}


/*
 * IG rendering
 */

static void _render_button(GRAPHICS_CONTROLLER *gc, BD_IG_BUTTON *button, BD_PG_PALETTE *palette,
                           int state)
{
    BD_PG_OBJECT *object    = NULL;
    BD_OVERLAY    ov;

    object = _find_object_for_button(gc->igs, button, state);
    if (!object) {
        TRACE("_render_button(#%d): object (state %d) not found\n", button->id, state);
        return;
    }

    ov.pts   = -1;
    ov.plane = 1; /* IG */

    ov.x = button->x_pos;
    ov.y = button->y_pos;
    ov.w = object->width;
    ov.h = object->height;

    ov.img     = object->img;
    ov.palette = palette->entry;

    if (gc->overlay_proc) {
        gc->overlay_proc(gc->overlay_proc_handle, &ov);
    }
}

static void _render_page(GRAPHICS_CONTROLLER *gc,
                         unsigned activated_button_id,
                         GC_NAV_CMDS *cmds)
{
    PG_DISPLAY_SET *s       = gc->igs;
    BD_IG_PAGE     *page    = NULL;
    BD_PG_PALETTE  *palette = NULL;
    unsigned        page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        ii;
    unsigned        selected_button_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);

    if (s->ics->interactive_composition.ui_model == 1 && !gc->popup_visible) {
        TRACE("_render_page(): popup menu not visible\n");

        _gc_clear_osd(gc, 1);

        return;
    }

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        ERROR("_render_page: unknown page id %d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return;
    }

    palette = _find_palette(s, page->palette_id_ref);
    if (!palette) {
        ERROR("_render_page: unknown palette id %d (have %d palettes)\n",
              page->palette_id_ref, s->num_palette);
        return;
    }

    TRACE("rendering page #%d using palette #%d. page has %d bogs\n",
          page->id, page->palette_id_ref, page->num_bogs);

    if (selected_button_id == 0xffff) {
        selected_button_id = page->default_selected_button_id_ref;
    }

    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BOG    *bog      = &page->bog[ii];
        unsigned      valid_id = gc->enabled_button[ii];
        BD_IG_BUTTON *button;

        button = _find_button_bog(bog, valid_id);

        if (!button) {
            TRACE("_render_page(): bog %d: button %d not found\n", ii, valid_id);

        } else if (button->id == activated_button_id) {
            _render_button(gc, button, palette, BTN_ACTIVATED);

        } else if (button->id == selected_button_id) {

            _render_button(gc, button, palette, BTN_SELECTED);

            bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, selected_button_id);

            if (button->auto_action_flag) {
                cmds->num_nav_cmds = button->num_nav_cmds;
                cmds->nav_cmds     = button->nav_cmds;
            }

        } else {
            _render_button(gc, button, palette, BTN_NORMAL);

        }
    }
}

/*
 * user actions
 */

#define VK_IS_NUMERIC(vk) (/*vk >= BD_VK_0  &&*/ vk <= BD_VK_9)
#define VK_IS_CURSOR(vk)  (vk >= BD_VK_UP && vk <= BD_VK_RIGHT)

static void _user_input(GRAPHICS_CONTROLLER *gc, bd_vk_key_e key, GC_NAV_CMDS *cmds)
{
    PG_DISPLAY_SET *s          = gc->igs;
    BD_IG_PAGE     *page       = NULL;
    unsigned        page_id    = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        cur_btn_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);
    unsigned        new_btn_id = cur_btn_id;
    unsigned        ii;
    int             activated_btn_id = -1;

    if (s->ics->interactive_composition.ui_model == 1 && !gc->popup_visible) {
        TRACE("_user_input(): popup menu not visible\n");
        return;
    }

    TRACE("_user_input(%d)\n", key);

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        ERROR("_user_input(): unknown page id %d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return;
    }

    if (key == BD_VK_MOUSE_ACTIVATE) {
        if (!gc->valid_mouse_position) {
            TRACE("_user_input(): BD_VK_MOUSE_ACTIVATE outside of valid buttons\n");
            return;
        }
        key = BD_VK_ENTER;
    }

    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BOG *bog      = &page->bog[ii];
        unsigned   valid_id = gc->enabled_button[ii];

        if (VK_IS_CURSOR(key) || key == BD_VK_ENTER) {
            BD_IG_BUTTON *button = _find_button_bog(bog, valid_id);
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
            if (VK_IS_NUMERIC(key)) {
                // TODO: numeric_select_value
            }

            if (new_btn_id != cur_btn_id) {
                BD_IG_BUTTON *button = _find_button_page(page, new_btn_id, NULL);
                if (button) {
                    cmds->sound_id_ref = button->selected_sound_id_ref;
                }
            }
        }
    }

    /* render page ? */
    if (new_btn_id != cur_btn_id || activated_btn_id >= 0 || !gc->ig_drawn) {

        bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, new_btn_id);

        _render_page(gc, activated_btn_id, cmds);
    }
}

static void _reset_enabled_button(GRAPHICS_CONTROLLER *gc)
{
    PG_DISPLAY_SET *s       = gc->igs;
    BD_IG_PAGE     *page    = NULL;
    unsigned        page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        ii;

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        ERROR("_reset_enabled_button(): unknown page #%d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return;
    }

    gc->enabled_button = realloc(gc->enabled_button,
                                 page->num_bogs * sizeof(uint16_t));

    for (ii = 0; ii < page->num_bogs; ii++) {
        gc->enabled_button[ii] = page->bog[ii].default_valid_button_id_ref;
    }
}

static void _set_button_page(GRAPHICS_CONTROLLER *gc, uint32_t param, GC_NAV_CMDS *cmds)
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

    TRACE("_set_button_page(0x%08x): page flag %d, id %d, effects %d   button flag %d, id %d",
          param, !!page_flag, page_id, !!effect_flag, !!button_flag, button_id);

    /* 10.4.3.4 (D) */

    if (!page_flag && !button_flag) {
        return;
    }

    if (page_flag) {

        /* current page --> command is ignored */
        if (page_id == bd_psr_read(gc->regs, PSR_MENU_PAGE_ID)) {
            TRACE("  page is current\n");
            return;
        }

        page = _find_page(&s->ics->interactive_composition, page_id);

        /* invalid page --> command is ignored */
        if (!page) {
            TRACE("  page is invalid\n");
            return;
        }

        /* page changes */

        bd_psr_write(gc->regs, PSR_MENU_PAGE_ID, page_id);

        _reset_enabled_button(gc);
        _gc_clear_osd(gc, 1);


    } else {
        /* page does not change */
        page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
        page    = _find_page(&s->ics->interactive_composition, page_id);

        if (!page) {
            ERROR("_set_button_page(): PSR_MENU_PAGE_ID refers to unknown page %d\n", page_id);
            return;
        }
    }

    if (button_flag) {
        /* find correct button and overlap group */
        button = _find_button_page(page, button_id, &bog_idx);

        if (!page_flag) {
            if (!button) {
                /* page not given, invalid button --> ignore command */
                TRACE("  button is invalid\n");
                return;
            }
            if (button_id == bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID)) {
                /* page not given, current button --> ignore command */
                TRACE("  button is current\n");
                return;
            }
        }
    }

    if (!button) {
        button_id = 0xffff; // run 5.9.7.4 and 5.9.8.3
    } else {
        gc->enabled_button[bog_idx] = button_id;
    }

    bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, button_id);

    gc->ig_drawn = 0;

    _render_page(gc, 0xffff, cmds);
}

static void _enable_button(GRAPHICS_CONTROLLER *gc, uint32_t button_id, unsigned enable)
{
    PG_DISPLAY_SET *s          = gc->igs;
    BD_IG_PAGE     *page       = NULL;
    BD_IG_BUTTON   *button     = NULL;
    unsigned        page_id    = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        cur_btn_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);
    unsigned        bog_idx    = 0;

    TRACE("_enable_button(#%d, %s)\n", button_id, enable ? "enable" : "disable");

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        TRACE("_enable_button(): unknown page #%d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return;
    }

    /* find correct button overlap group */
    button = _find_button_page(page, button_id, &bog_idx);
    if (!button) {
        TRACE("_enable_button(): unknown button #%d (page #%d)\n", button_id, page_id);
        return;
    }

    if (enable) {
        if (gc->enabled_button[bog_idx] == cur_btn_id) {
            /* selected button goes to disabled state */
            bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, 0x10000|button_id);
        }
        gc->enabled_button[bog_idx] = button_id;

    } else {
        if (gc->enabled_button[bog_idx] == button_id) {
            gc->enabled_button[bog_idx] = 0xffff;
        }

        if (cur_btn_id == button_id) {
            bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, 0xffff);
        }
    }
}

static int _is_button_enabled(GRAPHICS_CONTROLLER *gc, BD_IG_PAGE *page, unsigned button_id)
{
    unsigned ii;
    for (ii = 0; ii < page->num_bogs; ii++) {
        if (gc->enabled_button[ii] == button_id) {
            return 1;
        }
    }
    return 0;
}

static void _update_selected_button(GRAPHICS_CONTROLLER *gc)
{
    unsigned button_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);

    TRACE("_update_enabled_button(): currently enabled button is #%d\n", button_id);

    // special case: triggered only after enable button disables selected button
    if (button_id & 0x10000) {
        button_id &= 0xffff;
        bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, button_id);
        TRACE("_update_enabled_button() -> #%d [last enabled]\n", button_id);
        return;
    }

   if (button_id == 0xffff) {
        PG_DISPLAY_SET *s       = gc->igs;
        BD_IG_PAGE     *page    = NULL;
        unsigned        page_id = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);

        page = _find_page(&s->ics->interactive_composition, page_id);
        if (!page) {
            TRACE("_update_enabled_button(): unknown page #%d (have %d pages)\n",
                  page_id, s->ics->interactive_composition.num_pages);
            return;
        }

        // run 5.9.8.3

        if (_find_button_page(page, page->default_selected_button_id_ref, NULL) &&
            _is_button_enabled(gc, page, page->default_selected_button_id_ref)) {

            button_id = page->default_selected_button_id_ref;

        } else {
            unsigned ii;
            for (ii = 0; ii < page->num_bogs; ii++) {

                BD_IG_BOG *bog = &page->bog[ii];
                if (_find_button_bog(bog, gc->enabled_button[ii])) {
                    button_id = gc->enabled_button[ii];
                    break;
                }
            }
        }

        bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, button_id);
        TRACE("_update_enabled_button() -> #%d\n", button_id);
    }
}

static void _mouse_move(GRAPHICS_CONTROLLER *gc, unsigned x, unsigned y, GC_NAV_CMDS *cmds)
{
    PG_DISPLAY_SET *s          = gc->igs;
    BD_IG_PAGE     *page       = NULL;
    unsigned        page_id    = bd_psr_read(gc->regs, PSR_MENU_PAGE_ID);
    unsigned        cur_btn_id = bd_psr_read(gc->regs, PSR_SELECTED_BUTTON_ID);
    unsigned        new_btn_id = 0xffff;
    unsigned        ii;

    gc->valid_mouse_position = 0;

    page = _find_page(&s->ics->interactive_composition, page_id);
    if (!page) {
        ERROR("_mouse_move(): unknown page #%d (have %d pages)\n",
              page_id, s->ics->interactive_composition.num_pages);
        return;
    }

    for (ii = 0; ii < page->num_bogs; ii++) {
        BD_IG_BOG    *bog      = &page->bog[ii];
        unsigned      valid_id = gc->enabled_button[ii];
        BD_IG_BUTTON *button   = _find_button_bog(bog, valid_id);

        if (!button)
            continue;

        if (x < button->x_pos || y < button->y_pos)
            continue;

        BD_PG_OBJECT *object = _find_object_for_button(s, button, BTN_NORMAL);
        if (!object)
            continue;

        if (x >= button->x_pos + object->width || y >= button->y_pos + object->height)
            continue;

        // mouse is over button

        // is button already selected ?
        if (button->id == cur_btn_id) {
            gc->valid_mouse_position = 1;
            return;
        }

        // can button be selected ?
        if (!_find_object_for_button(s, button, BTN_SELECTED))
            return;

        new_btn_id = button->id;
        break;
    }

    if (new_btn_id != 0xffff) {
        bd_psr_write(gc->regs, PSR_SELECTED_BUTTON_ID, new_btn_id);

        _render_page(gc, -1, cmds);

        gc->valid_mouse_position = 1;
    }
}

void gc_run(GRAPHICS_CONTROLLER *gc, gc_ctrl_e ctrl, uint32_t param, GC_NAV_CMDS *cmds)
{
    if (cmds) {
        cmds->num_nav_cmds = 0;
        cmds->nav_cmds     = NULL;
        cmds->sound_id_ref = -1;
    }

    if (!gc || !gc->igs || !gc->igs->ics) {
        ERROR("gc_run(): no interactive composition\n");
        return;
    }

    switch (ctrl) {

        case GC_CTRL_SET_BUTTON_PAGE:
            _set_button_page(gc, param, cmds);
            break;

        case GC_CTRL_VK_KEY:
            if (param != BD_VK_POPUP) {
                _user_input(gc, param, cmds);
                break;
            }
            param = !gc->popup_visible;
            /* fall thru (BD_VK_POPUP) */

        case GC_CTRL_POPUP:
            if (!gc->igs || !gc->igs->ics || gc->igs->ics->interactive_composition.ui_model != 1) {
                /* not pop-up menu */
                break;
            }

            gc->popup_visible = !!param;

            if (gc->popup_visible) {
                bd_psr_write(gc->regs, PSR_MENU_PAGE_ID, 0);
                _reset_enabled_button(gc);
                _gc_clear_osd(gc, 1);
            }

            /* fall thru */

        case GC_CTRL_NOP:
            _render_page(gc, 0xffff, cmds);
            break;

        case GC_CTRL_RESET:
            _gc_reset(gc);
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
          _mouse_move(gc, param >> 16, param & 0xffff, cmds);
          return;
    }
}
