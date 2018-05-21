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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "util/macro.h"
#include "util/logging.h"
#include "libbluray/bluray.h"          /* bd_char_code_e */

#include <stdint.h>

#ifdef HAVE_FT2
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#endif

#include "textst_render.h"

/*
 *
 */

#define TEXTST_ERROR(...) BD_DEBUG(DBG_GC | DBG_CRIT, __VA_ARGS__)
#define TEXTST_TRACE(...) BD_DEBUG(DBG_GC,            __VA_ARGS__)

/*
 * data
 */
#ifdef HAVE_FT2
typedef struct {

  FT_Face  face;
  void    *mem;

} FONT_DATA;

struct textst_render {

  FT_Library     ft_lib;

  unsigned       font_count;
  FONT_DATA     *font;

  bd_char_code_e char_code;

};
#endif

/*
 * init / free
 */

TEXTST_RENDER *textst_render_init(void)
{
#ifdef HAVE_FT2
    TEXTST_RENDER *p = calloc(1, sizeof(TEXTST_RENDER));

    if (!p) {
        return NULL;
    }

    if (!FT_Init_FreeType(&p->ft_lib)) {
        return p;
    }

    X_FREE(p);
    TEXTST_ERROR("Loading FreeType2 failed\n");
#else
    TEXTST_ERROR("TextST font support not compiled in\n");
#endif
    return NULL;
}

void textst_render_free(TEXTST_RENDER **pp)
{
    if (pp && *pp) {
#ifdef HAVE_FT2
        TEXTST_RENDER *p = *pp;

        if (p->ft_lib) {
            /* free fonts */
            unsigned ii;
            for (ii = 0; ii < p->font_count; ii++) {
                if (p->font[ii].face) {
                    FT_Done_Face(p->font[ii].face);
                }
                X_FREE(p->font[ii].mem);
            }
            X_FREE(p->font);

            FT_Done_FreeType(p->ft_lib);
        }
#endif
        X_FREE(*pp);
    }
}

/*
 * settings
 */

int textst_render_add_font(TEXTST_RENDER *p, void *data, size_t size)
{
#ifdef HAVE_FT2
    FONT_DATA *tmp = realloc(p->font, sizeof(*(p->font)) * (p->font_count + 1));
    if (!tmp) {
        TEXTST_ERROR("out of memory\n");
        return -1;
    }
    p->font = tmp;

    if (FT_New_Memory_Face(p->ft_lib, (const FT_Byte*)data, (FT_Long)size, -1, NULL)) {
        TEXTST_ERROR("Unsupport font file format\n");
        return -1;
    }

    if (!FT_New_Memory_Face(p->ft_lib, (const FT_Byte*)data, (FT_Long)size, 0, &p->font[p->font_count].face)) {
        p->font[p->font_count].mem = data;
        p->font_count++;
        return 0;
    }

    TEXTST_ERROR("Loading font %d failed\n", p->font_count);

#else
    (void)p;
    (void)data;
    (void)size;
#endif

    return -1;
}

int textst_render_set_char_code(TEXTST_RENDER *p, int char_code)
{
#ifdef HAVE_FT2
    p->char_code = (bd_char_code_e)char_code;
    if (p->char_code != BLURAY_TEXT_CHAR_CODE_UTF8) {
        TEXTST_ERROR("WARNING: unsupported TextST coding type %d\n", char_code);
        return -1;
    }

#else
    (void)p;
    (void)char_code;
#endif

    return 0;
}

/*
 * UTF-8
 */

#ifdef HAVE_FT2

static int _utf8_char_size(const uint8_t *s)
{
    if ((s[0] & 0xE0) == 0xC0 &&
        (s[1] & 0xC0) == 0x80) {
        return 2;
    }
    if ((s[0] & 0xF0) == 0xE0 &&
        (s[1] & 0xC0) == 0x80 &&
        (s[2] & 0xC0) == 0x80) {
        return 3;
    }
    if ((s[0] & 0xF8) == 0xF0 &&
        (s[1] & 0xC0) == 0x80 &&
        (s[2] & 0xC0) == 0x80 &&
        (s[3] & 0xC0) == 0x80) {
        return 4;
    }
    return 1;
}

static unsigned _utf8_char_get(const uint8_t *s, int char_size)
{
    if (!char_size) {
        char_size = _utf8_char_size(s);
    }

    switch (char_size) {
        case 2: return ((s[0] & 0x1F) <<  6) | ((s[1] & 0x3F));
        case 3: return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) <<  6) | ((s[2] & 0x3F));
        case 4: return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | ((s[3] & 0x3F));
        default: ;
    }
    return s[0];
}

#endif /* HAVE_FT2 */

/*
 * rendering
 */

#ifdef HAVE_FT2

static int _draw_string(FT_Face face, const uint8_t *string, int length,
                        TEXTST_BITMAP *bmp, int x, int y,
                        BD_TEXTST_REGION_STYLE *style,
                        int *baseline_pos)
{
    uint8_t  color = style->font_color;
    unsigned char_code;
    int      ii;
    unsigned jj, kk;
    unsigned flags;

    if (length <= 0) {
        return -1;
    }
    if (!bmp) {
        flags = FT_LOAD_DEFAULT;
    } else {
        flags = FT_LOAD_RENDER;
    }

    for (ii = 0; ii < length; ii++) {
        /*if (p->char_code == BLURAY_TEXT_CHAR_CODE_UTF8) {*/
            int char_size = _utf8_char_size(string + ii);
            char_code = _utf8_char_get(string + ii, char_size);
            ii += char_size - 1;
        /*}*/

        if (FT_Load_Char(face, char_code, flags /*| FT_LOAD_MONOCHROME*/) == 0) {

            if (style->font_style.bold && !(face->style_flags & FT_STYLE_FLAG_BOLD)) {
                FT_GlyphSlot_Embolden( face->glyph );
            }
            if (style->font_style.italic && !(face->style_flags & FT_STYLE_FLAG_ITALIC)) {
                FT_GlyphSlot_Oblique( face->glyph );
            }

            if (bmp) {
                for (jj = 0; jj < face->glyph->bitmap.rows; jj++) {
                    for (kk = 0; kk < face->glyph->bitmap.width; kk++) {
                        uint8_t pixel = face->glyph->bitmap.buffer[jj * face->glyph->bitmap.pitch + kk];
                        if (pixel & 0x80) {
                            int xpos = x + face->glyph->bitmap_left + kk;
                            int ypos = y - face->glyph->bitmap_top + jj;
                            if (xpos >= 0 && xpos < bmp->width && ypos >= 0 && ypos < bmp->height) {
                                bmp->mem[xpos + ypos * bmp->stride] = color;
                            }
                        }
                    }
                }
            }

            /* track max baseline when calculating line size */
            if (baseline_pos) {
                *baseline_pos = BD_MAX(*baseline_pos, (face->size->metrics.ascender >> 6) + 1);
            }

            x += face->glyph->metrics.horiAdvance >> 6;
        }
    }

    return x;
}

static void _update_face(TEXTST_RENDER *p, FT_Face *face, const BD_TEXTST_REGION_STYLE *style)
{
    if (style->font_id_ref >= p->font_count || !p->font[style->font_id_ref].face) {
        TEXTST_ERROR("textst_Render: incorrect font index %d\n", style->font_id_ref);
        if (!*face) {
            *face = p->font[0].face;
        }
    } else {
        *face = p->font[style->font_id_ref].face;
    }
    FT_Set_Char_Size(*face, 0, style->font_size << 6, 0, 0);
}

static int _render_line(TEXTST_RENDER *p, TEXTST_BITMAP *bmp,
                        const BD_TEXTST_REGION_STYLE *base_style,
                        BD_TEXTST_REGION_STYLE *style,
                        uint8_t **p_ptr, int *p_elem_count,
                        int xpos, int ypos, int *baseline_pos)
{
    FT_Face  face = NULL;

    /* select font */
    _update_face(p, &face, style);

    while ( (*p_elem_count) > 0) {
        BD_TEXTST_DATA *elem = (BD_TEXTST_DATA*)*p_ptr;
        (*p_ptr) += sizeof(BD_TEXTST_DATA);
        (*p_elem_count)--;

        switch (elem->type) {
            case BD_TEXTST_DATA_STRING:
                xpos = _draw_string(face, elem->data.text.string, elem->data.text.length,
                                    bmp, xpos, ypos, style, baseline_pos);
                (*p_ptr) += elem->data.text.length;
                break;

            case BD_TEXTST_DATA_NEWLINE:
                return xpos;

            case BD_TEXTST_DATA_FONT_ID:
                style->font_id_ref = elem->data.font_id_ref;
                _update_face(p, &face, style);
                break;

            case BD_TEXTST_DATA_FONT_STYLE:
                style->font_style =  elem->data.style.style;
                style->outline_color = elem->data.style.outline_color;
                style->outline_thickness = elem->data.style.outline_thickness;
                if (style->font_style.outline_border) {
                    TEXTST_ERROR("textst_render: unsupported style: outline\n");
                }
                break;

            case BD_TEXTST_DATA_FONT_SIZE:
                style->font_size = elem->data.font_size;
                _update_face(p, &face, style);
                break;

            case BD_TEXTST_DATA_FONT_COLOR:
                style->font_color = elem->data.font_color;
                break;

            case BD_TEXTST_DATA_RESET_STYLE:
                memcpy(style, base_style, sizeof(*style));
                _update_face(p, &face, style);
                break;

            default:
                TEXTST_ERROR("Unknown control code %d\n", elem->type);
                break;
        }
    }

    return xpos;
}

#endif /* HAVE_FT2 */

int textst_render(TEXTST_RENDER *p,
                  TEXTST_BITMAP *bmp,
                  const BD_TEXTST_REGION_STYLE *base_style,
                  const BD_TEXTST_DIALOG_REGION *region)
{
#ifdef HAVE_FT2

    /* fonts loaded ? */
    if (p->font_count < 1) {
        TEXTST_ERROR("textst_render: no fonts loaded\n");
        return -1;
    }

    /* TODO: */
    if (base_style->text_flow != BD_TEXTST_FLOW_LEFT_RIGHT) {
        TEXTST_ERROR("textst_render: unsupported text flow type %d\n", base_style->text_flow);
    }
    if (bmp->argb) {
        TEXTST_ERROR("textst_render: ARGB output not implemented\n");
        return -1;
    }
    if (base_style->font_style.outline_border) {
        /* TODO: styles: see ex. vlc/modules/text_renderer/freetype.c ; function GetGlyph() */
        TEXTST_ERROR("textst_render: unsupported style: outline\n");
    }

    /* */

    BD_TEXTST_REGION_STYLE s;   /* current style settings */
    unsigned  line;
    uint8_t  *ptr = (uint8_t*)region->elem;
    int       elem_count = region->elem_count;
    int       xpos = 0;
    int       ypos = 0;

    /* settings can be changed and reset with inline codes. Use local copy. */
    memcpy(&s, base_style, sizeof(s));

    /* apply vertical alignment */
    switch (s.text_valign) {
        case BD_TEXTST_VALIGN_TOP:
            break;
        case BD_TEXTST_VALIGN_BOTTOM:
            ypos = s.text_box.height - region->line_count * s.line_space;
            break;
        case BD_TEXTST_VALIGN_MIDDLE:
            ypos = (s.text_box.height - region->line_count * s.line_space) / 2;
            break;
        default:
            TEXTST_ERROR("textst_render: unsupported vertical align %d\n", s.text_halign);
            break;
    }

    for (line = 0; line < region->line_count; line++) {

        /* calculate line width and max. ascender */
        uint8_t *ptr_tmp = ptr;
        int elem_count_tmp = elem_count;
        BD_TEXTST_REGION_STYLE style_tmp;
        int baseline = 0, line_width;

        /* dry-run: count line width and height */
        memcpy(&style_tmp, &s, sizeof(s)); /* use copy in first pass */
        line_width = _render_line(p, NULL, base_style, &style_tmp, &ptr_tmp, &elem_count_tmp, 0, 0, &baseline);

        /* adjust to baseline */
        ypos += baseline;

        /* apply horizontal alignment */
        xpos = 0;
        switch (s.text_halign) {
            case BD_TEXTST_HALIGN_LEFT:
                break;
            case BD_TEXTST_HALIGN_RIGHT:
                xpos = s.text_box.width - line_width - 1;
                break;
            case BD_TEXTST_HALIGN_CENTER:
                xpos = (s.text_box.width - line_width) / 2 - 1;
                break;
            default:
                TEXTST_ERROR("textst_render: unsupported horizontal align %d\n", s.text_halign);
                break;
        }

        /* render line */
        _render_line(p, bmp, base_style, &s, &ptr, &elem_count, xpos, ypos, NULL);

        ypos += s.line_space - baseline;
    }

#else
    (void)p;
    (void)bmp;
    (void)base_style;
    (void)region;
#endif /* HAVE_FT2 */

    return 0;
}
