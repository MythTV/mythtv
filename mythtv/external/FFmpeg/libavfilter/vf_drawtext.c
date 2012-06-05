/*
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2010 S.N. Hemanth Meenakshisundaram
 * Copyright (c) 2003 Gustavo Sverzut Barbieri <gsbarbieri@yahoo.com.br>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * drawtext filter, based on the original vhook/drawtext.c
 * filter by Gustavo Sverzut Barbieri
 */

#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "libavutil/avstring.h"
#include "libavutil/file.h"
#include "libavutil/eval.h"
#include "libavutil/opt.h"
#include "libavutil/random_seed.h"
#include "libavutil/parseutils.h"
#include "libavutil/timecode.h"
#include "libavutil/tree.h"
#include "libavutil/lfg.h"
#include "avfilter.h"
#include "drawutils.h"
#include "video.h"

#undef time

#include <ft2build.h>
#include <freetype/config/ftheader.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#if CONFIG_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

static const char *const var_names[] = {
    "dar",
    "hsub", "vsub",
    "line_h", "lh",           ///< line height, same as max_glyph_h
    "main_h", "h", "H",       ///< height of the input video
    "main_w", "w", "W",       ///< width  of the input video
    "max_glyph_a", "ascent",  ///< max glyph ascent
    "max_glyph_d", "descent", ///< min glyph descent
    "max_glyph_h",            ///< max glyph height
    "max_glyph_w",            ///< max glyph width
    "n",                      ///< number of frame
    "sar",
    "t",                      ///< timestamp expressed in seconds
    "text_h", "th",           ///< height of the rendered text
    "text_w", "tw",           ///< width  of the rendered text
    "x",
    "y",
    NULL
};

static const char *const fun2_names[] = {
    "rand"
};

static double drand(void *opaque, double min, double max)
{
    return min + (max-min) / UINT_MAX * av_lfg_get(opaque);
}

typedef double (*eval_func2)(void *, double a, double b);

static const eval_func2 fun2[] = {
    drand,
    NULL
};

enum var_name {
    VAR_DAR,
    VAR_HSUB, VAR_VSUB,
    VAR_LINE_H, VAR_LH,
    VAR_MAIN_H, VAR_h, VAR_H,
    VAR_MAIN_W, VAR_w, VAR_W,
    VAR_MAX_GLYPH_A, VAR_ASCENT,
    VAR_MAX_GLYPH_D, VAR_DESCENT,
    VAR_MAX_GLYPH_H,
    VAR_MAX_GLYPH_W,
    VAR_N,
    VAR_SAR,
    VAR_T,
    VAR_TEXT_H, VAR_TH,
    VAR_TEXT_W, VAR_TW,
    VAR_X,
    VAR_Y,
    VAR_VARS_NB
};

typedef struct {
    const AVClass *class;
    int reinit;                     ///< tells if the filter is being reinited
    uint8_t *fontfile;              ///< font to be used
    uint8_t *text;                  ///< text to be drawn
    uint8_t *expanded_text;         ///< used to contain the strftime()-expanded text
    size_t   expanded_text_size;    ///< size in bytes of the expanded_text buffer
    int ft_load_flags;              ///< flags used for loading fonts, see FT_LOAD_*
    FT_Vector *positions;           ///< positions for each element in the text
    size_t nb_positions;            ///< number of elements of positions array
    char *textfile;                 ///< file with text to be drawn
    int x;                          ///< x position to start drawing text
    int y;                          ///< y position to start drawing text
    int max_glyph_w;                ///< max glyph width
    int max_glyph_h;                ///< max glyph height
    int shadowx, shadowy;
    unsigned int fontsize;          ///< font size to use
    char *fontcolor_string;         ///< font color as string
    char *boxcolor_string;          ///< box color as string
    char *shadowcolor_string;       ///< shadow color as string

    short int draw_box;             ///< draw box around text - true or false
    int use_kerning;                ///< font kerning is used - true/false
    int tabsize;                    ///< tab size
    int fix_bounds;                 ///< do we let it go out of frame bounds - t/f

    FFDrawContext dc;
    FFDrawColor fontcolor;          ///< foreground color
    FFDrawColor shadowcolor;        ///< shadow color
    FFDrawColor boxcolor;           ///< background color

    FT_Library library;             ///< freetype font library handle
    FT_Face face;                   ///< freetype font face handle
    struct AVTreeNode *glyphs;      ///< rendered glyphs, stored using the UTF-32 char code
    char *x_expr;                   ///< expression for x position
    char *y_expr;                   ///< expression for y position
    AVExpr *x_pexpr, *y_pexpr;      ///< parsed expressions for x and y
    int64_t basetime;               ///< base pts time in the real world for display
    double var_values[VAR_VARS_NB];
    char   *draw_expr;              ///< expression for draw
    AVExpr *draw_pexpr;             ///< parsed expression for draw
    int draw;                       ///< set to zero to prevent drawing
    AVLFG  prng;                    ///< random
    char       *tc_opt_string;      ///< specified timecode option string
    AVRational  tc_rate;            ///< frame rate for timecode
    AVTimecode  tc;                 ///< timecode context
    int tc24hmax;                   ///< 1 if timecode is wrapped to 24 hours, 0 otherwise
    int frame_id;
} DrawTextContext;

#define OFFSET(x) offsetof(DrawTextContext, x)

static const AVOption drawtext_options[]= {
{"fontfile", "set font file",        OFFSET(fontfile),           AV_OPT_TYPE_STRING, {.str=NULL},  CHAR_MIN, CHAR_MAX },
{"text",     "set text",             OFFSET(text),               AV_OPT_TYPE_STRING, {.str=NULL},  CHAR_MIN, CHAR_MAX },
{"textfile", "set text file",        OFFSET(textfile),           AV_OPT_TYPE_STRING, {.str=NULL},  CHAR_MIN, CHAR_MAX },
{"fontcolor",   "set foreground color", OFFSET(fontcolor_string),   AV_OPT_TYPE_STRING, {.str="black"}, CHAR_MIN, CHAR_MAX },
{"boxcolor",    "set box color",        OFFSET(boxcolor_string),    AV_OPT_TYPE_STRING, {.str="white"}, CHAR_MIN, CHAR_MAX },
{"shadowcolor", "set shadow color",     OFFSET(shadowcolor_string), AV_OPT_TYPE_STRING, {.str="black"}, CHAR_MIN, CHAR_MAX },
{"box",      "set box",              OFFSET(draw_box),           AV_OPT_TYPE_INT,    {.dbl=0},     0,        1        },
{"fontsize", "set font size",        OFFSET(fontsize),           AV_OPT_TYPE_INT,    {.dbl=0},     0,        INT_MAX  },
{"x",        "set x expression",     OFFSET(x_expr),             AV_OPT_TYPE_STRING, {.str="0"},   CHAR_MIN, CHAR_MAX },
{"y",        "set y expression",     OFFSET(y_expr),             AV_OPT_TYPE_STRING, {.str="0"},   CHAR_MIN, CHAR_MAX },
{"shadowx",  "set x",                OFFSET(shadowx),            AV_OPT_TYPE_INT,    {.dbl=0},     INT_MIN,  INT_MAX  },
{"shadowy",  "set y",                OFFSET(shadowy),            AV_OPT_TYPE_INT,    {.dbl=0},     INT_MIN,  INT_MAX  },
{"tabsize",  "set tab size",         OFFSET(tabsize),            AV_OPT_TYPE_INT,    {.dbl=4},     0,        INT_MAX  },
{"basetime", "set base time",        OFFSET(basetime),           AV_OPT_TYPE_INT64,  {.dbl=AV_NOPTS_VALUE},     INT64_MIN,        INT64_MAX  },
{"draw",     "if false do not draw", OFFSET(draw_expr),          AV_OPT_TYPE_STRING, {.str="1"},   CHAR_MIN, CHAR_MAX },
{"timecode", "set initial timecode", OFFSET(tc_opt_string),      AV_OPT_TYPE_STRING, {.str=NULL},  CHAR_MIN, CHAR_MAX },
{"tc24hmax", "set 24 hours max (timecode only)", OFFSET(tc24hmax), AV_OPT_TYPE_INT,  {.dbl=0},            0,        1 },
{"timecode_rate", "set rate (timecode only)", OFFSET(tc_rate),   AV_OPT_TYPE_RATIONAL, {.dbl=0},          0,  INT_MAX },
{"r",        "set rate (timecode only)", OFFSET(tc_rate),        AV_OPT_TYPE_RATIONAL, {.dbl=0},          0,  INT_MAX },
{"rate",     "set rate (timecode only)", OFFSET(tc_rate),        AV_OPT_TYPE_RATIONAL, {.dbl=0},          0,  INT_MAX },
{"fix_bounds", "if true, check and fix text coords to avoid clipping",
                                     OFFSET(fix_bounds),         AV_OPT_TYPE_INT,    {.dbl=1},     0,        1        },

/* FT_LOAD_* flags */
{"ft_load_flags", "set font loading flags for libfreetype",   OFFSET(ft_load_flags),  AV_OPT_TYPE_FLAGS,  {.dbl=FT_LOAD_DEFAULT|FT_LOAD_RENDER}, 0, INT_MAX, 0, "ft_load_flags" },
{"default",                     "set default",                     0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_DEFAULT},                     INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"no_scale",                    "set no_scale",                    0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_NO_SCALE},                    INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"no_hinting",                  "set no_hinting",                  0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_NO_HINTING},                  INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"render",                      "set render",                      0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_RENDER},                      INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"no_bitmap",                   "set no_bitmap",                   0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_NO_BITMAP},                   INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"vertical_layout",             "set vertical_layout",             0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_VERTICAL_LAYOUT},             INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"force_autohint",              "set force_autohint",              0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_FORCE_AUTOHINT},              INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"crop_bitmap",                 "set crop_bitmap",                 0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_CROP_BITMAP},                 INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"pedantic",                    "set pedantic",                    0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_PEDANTIC},                    INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"ignore_global_advance_width", "set ignore_global_advance_width", 0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH}, INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"no_recurse",                  "set no_recurse",                  0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_NO_RECURSE},                  INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"ignore_transform",            "set ignore_transform",            0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_IGNORE_TRANSFORM},            INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"monochrome",                  "set monochrome",                  0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_MONOCHROME},                  INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"linear_design",               "set linear_design",               0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_LINEAR_DESIGN},               INT_MIN, INT_MAX, 0, "ft_load_flags" },
{"no_autohint",                 "set no_autohint",                 0, AV_OPT_TYPE_CONST, {.dbl=FT_LOAD_NO_AUTOHINT},                 INT_MIN, INT_MAX, 0, "ft_load_flags" },
{NULL},
};

static const char *drawtext_get_name(void *ctx)
{
    return "drawtext";
}

static const AVClass drawtext_class = {
    "DrawTextContext",
    drawtext_get_name,
    drawtext_options
};

#undef __FTERRORS_H__
#define FT_ERROR_START_LIST {
#define FT_ERRORDEF(e, v, s) { (e), (s) },
#define FT_ERROR_END_LIST { 0, NULL } };

struct ft_error
{
    int err;
    const char *err_msg;
} static ft_errors[] =
#include FT_ERRORS_H

#define FT_ERRMSG(e) ft_errors[e].err_msg

typedef struct {
    FT_Glyph *glyph;
    uint32_t code;
    FT_Bitmap bitmap; ///< array holding bitmaps of font
    FT_BBox bbox;
    int advance;
    int bitmap_left;
    int bitmap_top;
} Glyph;

static int glyph_cmp(void *key, const void *b)
{
    const Glyph *a = key, *bb = b;
    int64_t diff = (int64_t)a->code - (int64_t)bb->code;
    return diff > 0 ? 1 : diff < 0 ? -1 : 0;
}

/**
 * Load glyphs corresponding to the UTF-32 codepoint code.
 */
static int load_glyph(AVFilterContext *ctx, Glyph **glyph_ptr, uint32_t code)
{
    DrawTextContext *dtext = ctx->priv;
    Glyph *glyph;
    struct AVTreeNode *node = NULL;
    int ret;

    /* load glyph into dtext->face->glyph */
    if (FT_Load_Char(dtext->face, code, dtext->ft_load_flags))
        return AVERROR(EINVAL);

    /* save glyph */
    if (!(glyph = av_mallocz(sizeof(*glyph))) ||
        !(glyph->glyph = av_mallocz(sizeof(*glyph->glyph)))) {
        ret = AVERROR(ENOMEM);
        goto error;
    }
    glyph->code  = code;

    if (FT_Get_Glyph(dtext->face->glyph, glyph->glyph)) {
        ret = AVERROR(EINVAL);
        goto error;
    }

    glyph->bitmap      = dtext->face->glyph->bitmap;
    glyph->bitmap_left = dtext->face->glyph->bitmap_left;
    glyph->bitmap_top  = dtext->face->glyph->bitmap_top;
    glyph->advance     = dtext->face->glyph->advance.x >> 6;

    /* measure text height to calculate text_height (or the maximum text height) */
    FT_Glyph_Get_CBox(*glyph->glyph, ft_glyph_bbox_pixels, &glyph->bbox);

    /* cache the newly created glyph */
    if (!(node = av_mallocz(av_tree_node_size))) {
        ret = AVERROR(ENOMEM);
        goto error;
    }
    av_tree_insert(&dtext->glyphs, glyph, glyph_cmp, &node);

    if (glyph_ptr)
        *glyph_ptr = glyph;
    return 0;

error:
    if (glyph)
        av_freep(&glyph->glyph);
    av_freep(&glyph);
    av_freep(&node);
    return ret;
}

static int load_font_file(AVFilterContext *ctx, const char *path, int index,
                          const char **error)
{
    DrawTextContext *dtext = ctx->priv;
    int err;

    err = FT_New_Face(dtext->library, path, index, &dtext->face);
    if (err) {
        *error = FT_ERRMSG(err);
        return AVERROR(EINVAL);
    }
    return 0;
}

#if CONFIG_FONTCONFIG
static int load_font_fontconfig(AVFilterContext *ctx, const char **error)
{
    DrawTextContext *dtext = ctx->priv;
    FcConfig *fontconfig;
    FcPattern *pattern, *fpat;
    FcResult result = FcResultMatch;
    FcChar8 *filename;
    int err, index;
    double size;

    fontconfig = FcInitLoadConfigAndFonts();
    if (!fontconfig) {
        *error = "impossible to init fontconfig\n";
        return AVERROR(EINVAL);
    }
    pattern = FcNameParse(dtext->fontfile ? dtext->fontfile :
                          (uint8_t *)(intptr_t)"default");
    if (!pattern) {
        *error = "could not parse fontconfig pattern";
        return AVERROR(EINVAL);
    }
    if (!FcConfigSubstitute(fontconfig, pattern, FcMatchPattern)) {
        *error = "could not substitue fontconfig options"; /* very unlikely */
        return AVERROR(EINVAL);
    }
    FcDefaultSubstitute(pattern);
    fpat = FcFontMatch(fontconfig, pattern, &result);
    if (!fpat || result != FcResultMatch) {
        *error = "impossible to find a matching font";
        return AVERROR(EINVAL);
    }
    if (FcPatternGetString (fpat, FC_FILE,  0, &filename) != FcResultMatch ||
        FcPatternGetInteger(fpat, FC_INDEX, 0, &index   ) != FcResultMatch ||
        FcPatternGetDouble (fpat, FC_SIZE,  0, &size    ) != FcResultMatch) {
        *error = "impossible to find font information";
        return AVERROR(EINVAL);
    }
    av_log(ctx, AV_LOG_INFO, "Using \"%s\"\n", filename);
    if (!dtext->fontsize)
        dtext->fontsize = size + 0.5;
    err = load_font_file(ctx, filename, index, error);
    if (err)
        return err;
    FcPatternDestroy(fpat);
    FcPatternDestroy(pattern);
    FcConfigDestroy(fontconfig);
    return 0;
}
#endif

static int load_font(AVFilterContext *ctx)
{
    DrawTextContext *dtext = ctx->priv;
    int err;
    const char *error = "unknown error\n";

    /* load the face, and set up the encoding, which is by default UTF-8 */
    err = load_font_file(ctx, dtext->fontfile, 0, &error);
    if (!err)
        return 0;
#if CONFIG_FONTCONFIG
    err = load_font_fontconfig(ctx, &error);
    if (!err)
        return 0;
#endif
    av_log(ctx, AV_LOG_ERROR, "Could not load font \"%s\": %s\n",
           dtext->fontfile, error);
    return err;
}

static av_cold int init(AVFilterContext *ctx, const char *args, void *opaque)
{
    int err;
    DrawTextContext *dtext = ctx->priv;
    Glyph *glyph;

    dtext->class = &drawtext_class;
    av_opt_set_defaults(dtext);

    if ((err = (av_set_options_string(dtext, args, "=", ":"))) < 0) {
        av_log(ctx, AV_LOG_ERROR, "Error parsing options string: '%s'\n", args);
        return err;
    }

    if (!dtext->fontfile && !CONFIG_FONTCONFIG) {
        av_log(ctx, AV_LOG_ERROR, "No font filename provided\n");
        return AVERROR(EINVAL);
    }

    if (dtext->textfile) {
        uint8_t *textbuf;
        size_t textbuf_size;

        if (dtext->text) {
            av_log(ctx, AV_LOG_ERROR,
                   "Both text and text file provided. Please provide only one\n");
            return AVERROR(EINVAL);
        }
        if ((err = av_file_map(dtext->textfile, &textbuf, &textbuf_size, 0, ctx)) < 0) {
            av_log(ctx, AV_LOG_ERROR,
                   "The text file '%s' could not be read or is empty\n",
                   dtext->textfile);
            return err;
        }

        if (!(dtext->text = av_malloc(textbuf_size+1)))
            return AVERROR(ENOMEM);
        memcpy(dtext->text, textbuf, textbuf_size);
        dtext->text[textbuf_size] = 0;
        av_file_unmap(textbuf, textbuf_size);
    }

    if (dtext->tc_opt_string) {
        int ret = av_timecode_init_from_string(&dtext->tc, dtext->tc_rate,
                                               dtext->tc_opt_string, ctx);
        if (ret < 0)
            return ret;
        if (dtext->tc24hmax)
            dtext->tc.flags |= AV_TIMECODE_FLAG_24HOURSMAX;
        if (!dtext->text)
            dtext->text = av_strdup("");
    }

    if (!dtext->text) {
        av_log(ctx, AV_LOG_ERROR,
               "Either text, a valid file or a timecode must be provided\n");
        return AVERROR(EINVAL);
    }

    if ((err = av_parse_color(dtext->fontcolor.rgba, dtext->fontcolor_string, -1, ctx))) {
        av_log(ctx, AV_LOG_ERROR,
               "Invalid font color '%s'\n", dtext->fontcolor_string);
        return err;
    }

    if ((err = av_parse_color(dtext->boxcolor.rgba, dtext->boxcolor_string, -1, ctx))) {
        av_log(ctx, AV_LOG_ERROR,
               "Invalid box color '%s'\n", dtext->boxcolor_string);
        return err;
    }

    if ((err = av_parse_color(dtext->shadowcolor.rgba, dtext->shadowcolor_string, -1, ctx))) {
        av_log(ctx, AV_LOG_ERROR,
               "Invalid shadow color '%s'\n", dtext->shadowcolor_string);
        return err;
    }

    if ((err = FT_Init_FreeType(&(dtext->library)))) {
        av_log(ctx, AV_LOG_ERROR,
               "Could not load FreeType: %s\n", FT_ERRMSG(err));
        return AVERROR(EINVAL);
    }

    err = load_font(ctx);
    if (err)
        return err;
    if (!dtext->fontsize)
        dtext->fontsize = 16;
    if ((err = FT_Set_Pixel_Sizes(dtext->face, 0, dtext->fontsize))) {
        av_log(ctx, AV_LOG_ERROR, "Could not set font size to %d pixels: %s\n",
               dtext->fontsize, FT_ERRMSG(err));
        return AVERROR(EINVAL);
    }

    dtext->use_kerning = FT_HAS_KERNING(dtext->face);

    /* load the fallback glyph with code 0 */
    load_glyph(ctx, NULL, 0);

    /* set the tabsize in pixels */
    if ((err = load_glyph(ctx, &glyph, ' ')) < 0) {
        av_log(ctx, AV_LOG_ERROR, "Could not set tabsize.\n");
        return err;
    }
    dtext->tabsize *= glyph->advance;

    return 0;
}

static int query_formats(AVFilterContext *ctx)
{
    avfilter_set_common_pixel_formats(ctx, ff_draw_supported_pixel_formats(0));
    return 0;
}

static int glyph_enu_free(void *opaque, void *elem)
{
    Glyph *glyph = elem;

    FT_Done_Glyph(*glyph->glyph);
    av_freep(&glyph->glyph);
    av_free(elem);
    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    DrawTextContext *dtext = ctx->priv;

    av_expr_free(dtext->x_pexpr); dtext->x_pexpr = NULL;
    av_expr_free(dtext->y_pexpr); dtext->y_pexpr = NULL;
    av_expr_free(dtext->draw_pexpr); dtext->draw_pexpr = NULL;

    av_freep(&dtext->boxcolor_string);
    av_freep(&dtext->expanded_text);
    av_freep(&dtext->fontcolor_string);
    av_freep(&dtext->fontfile);
    av_freep(&dtext->shadowcolor_string);
    av_freep(&dtext->text);
    av_freep(&dtext->x_expr);
    av_freep(&dtext->y_expr);
    av_freep(&dtext->draw_expr);

    av_freep(&dtext->positions);
    dtext->nb_positions = 0;

    av_tree_enumerate(dtext->glyphs, NULL, NULL, glyph_enu_free);
    av_tree_destroy(dtext->glyphs);
    dtext->glyphs = NULL;

    FT_Done_Face(dtext->face);
    FT_Done_FreeType(dtext->library);
}

static inline int is_newline(uint32_t c)
{
    return c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int config_input(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    DrawTextContext *dtext = ctx->priv;
    int ret;

    ff_draw_init(&dtext->dc, inlink->format, 0);
    ff_draw_color(&dtext->dc, &dtext->fontcolor,   dtext->fontcolor.rgba);
    ff_draw_color(&dtext->dc, &dtext->shadowcolor, dtext->shadowcolor.rgba);
    ff_draw_color(&dtext->dc, &dtext->boxcolor,    dtext->boxcolor.rgba);

    dtext->var_values[VAR_w]     = dtext->var_values[VAR_W]     = dtext->var_values[VAR_MAIN_W] = inlink->w;
    dtext->var_values[VAR_h]     = dtext->var_values[VAR_H]     = dtext->var_values[VAR_MAIN_H] = inlink->h;
    dtext->var_values[VAR_SAR]   = inlink->sample_aspect_ratio.num ? av_q2d(inlink->sample_aspect_ratio) : 1;
    dtext->var_values[VAR_DAR]   = (double)inlink->w / inlink->h * dtext->var_values[VAR_SAR];
    dtext->var_values[VAR_HSUB]  = 1 << dtext->dc.hsub_max;
    dtext->var_values[VAR_VSUB]  = 1 << dtext->dc.vsub_max;
    dtext->var_values[VAR_X]     = NAN;
    dtext->var_values[VAR_Y]     = NAN;
    if (!dtext->reinit)
        dtext->var_values[VAR_N] = 0;
    dtext->var_values[VAR_T]     = NAN;

    av_lfg_init(&dtext->prng, av_get_random_seed());

    if ((ret = av_expr_parse(&dtext->x_pexpr, dtext->x_expr, var_names,
                             NULL, NULL, fun2_names, fun2, 0, ctx)) < 0 ||
        (ret = av_expr_parse(&dtext->y_pexpr, dtext->y_expr, var_names,
                             NULL, NULL, fun2_names, fun2, 0, ctx)) < 0 ||
        (ret = av_expr_parse(&dtext->draw_pexpr, dtext->draw_expr, var_names,
                             NULL, NULL, fun2_names, fun2, 0, ctx)) < 0)

        return AVERROR(EINVAL);

    return 0;
}

static int command(AVFilterContext *ctx, const char *cmd, const char *arg, char *res, int res_len, int flags)
{
    DrawTextContext *dtext = ctx->priv;

    if (!strcmp(cmd, "reinit")) {
        int ret;
        uninit(ctx);
        dtext->reinit = 1;
        if ((ret = init(ctx, arg, NULL)) < 0)
            return ret;
        return config_input(ctx->inputs[0]);
    }

    return AVERROR(ENOSYS);
}

static int draw_glyphs(DrawTextContext *dtext, AVFilterBufferRef *picref,
                       int width, int height, const uint8_t rgbcolor[4], FFDrawColor *color, int x, int y)
{
    char *text = dtext->expanded_text;
    uint32_t code = 0;
    int i, x1, y1;
    uint8_t *p;
    Glyph *glyph = NULL;

    for (i = 0, p = text; *p; i++) {
        Glyph dummy = { 0 };
        GET_UTF8(code, *p++, continue;);

        /* skip new line chars, just go to new line */
        if (code == '\n' || code == '\r' || code == '\t')
            continue;

        dummy.code = code;
        glyph = av_tree_find(dtext->glyphs, &dummy, (void *)glyph_cmp, NULL);

        if (glyph->bitmap.pixel_mode != FT_PIXEL_MODE_MONO &&
            glyph->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
            return AVERROR(EINVAL);

        x1 = dtext->positions[i].x+dtext->x+x;
        y1 = dtext->positions[i].y+dtext->y+y;

        ff_blend_mask(&dtext->dc, color,
                      picref->data, picref->linesize, width, height,
                      glyph->bitmap.buffer, glyph->bitmap.pitch,
                      glyph->bitmap.width, glyph->bitmap.rows,
                      glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO ? 0 : 3,
                      0, x1, y1);
    }

    return 0;
}

static int draw_text(AVFilterContext *ctx, AVFilterBufferRef *picref,
                     int width, int height)
{
    DrawTextContext *dtext = ctx->priv;
    uint32_t code = 0, prev_code = 0;
    int x = 0, y = 0, i = 0, ret;
    int max_text_line_w = 0, len;
    int box_w, box_h;
    char *text = dtext->text;
    uint8_t *p;
    int y_min = 32000, y_max = -32000;
    int x_min = 32000, x_max = -32000;
    FT_Vector delta;
    Glyph *glyph = NULL, *prev_glyph = NULL;
    Glyph dummy = { 0 };

    time_t now = time(0);
    struct tm ltime;
    uint8_t *buf = dtext->expanded_text;
    int buf_size = dtext->expanded_text_size;

    if(dtext->basetime != AV_NOPTS_VALUE)
        now= picref->pts*av_q2d(ctx->inputs[0]->time_base) + dtext->basetime/1000000;

    if (!buf) {
        buf_size = 2*strlen(dtext->text)+1;
        buf = av_malloc(buf_size);
    }

#if HAVE_LOCALTIME_R
    localtime_r(&now, &ltime);
#else
    if(strchr(dtext->text, '%'))
        ltime= *localtime(&now);
#endif

    do {
        *buf = 1;
        if (strftime(buf, buf_size, dtext->text, &ltime) != 0 || *buf == 0)
            break;
        buf_size *= 2;
    } while ((buf = av_realloc(buf, buf_size)));

    if (dtext->tc_opt_string) {
        char tcbuf[AV_TIMECODE_STR_SIZE];
        av_timecode_make_string(&dtext->tc, tcbuf, dtext->frame_id++);
        buf = av_asprintf("%s%s", dtext->text, tcbuf);
    }

    if (!buf)
        return AVERROR(ENOMEM);
    text = dtext->expanded_text = buf;
    dtext->expanded_text_size = buf_size;
    if ((len = strlen(text)) > dtext->nb_positions) {
        if (!(dtext->positions =
              av_realloc(dtext->positions, len*sizeof(*dtext->positions))))
            return AVERROR(ENOMEM);
        dtext->nb_positions = len;
    }

    x = 0;
    y = 0;

    /* load and cache glyphs */
    for (i = 0, p = text; *p; i++) {
        GET_UTF8(code, *p++, continue;);

        /* get glyph */
        dummy.code = code;
        glyph = av_tree_find(dtext->glyphs, &dummy, glyph_cmp, NULL);
        if (!glyph) {
            load_glyph(ctx, &glyph, code);
        }

        y_min = FFMIN(glyph->bbox.yMin, y_min);
        y_max = FFMAX(glyph->bbox.yMax, y_max);
        x_min = FFMIN(glyph->bbox.xMin, x_min);
        x_max = FFMAX(glyph->bbox.xMax, x_max);
    }
    dtext->max_glyph_h = y_max - y_min;
    dtext->max_glyph_w = x_max - x_min;

    /* compute and save position for each glyph */
    glyph = NULL;
    for (i = 0, p = text; *p; i++) {
        GET_UTF8(code, *p++, continue;);

        /* skip the \n in the sequence \r\n */
        if (prev_code == '\r' && code == '\n')
            continue;

        prev_code = code;
        if (is_newline(code)) {
            max_text_line_w = FFMAX(max_text_line_w, x);
            y += dtext->max_glyph_h;
            x = 0;
            continue;
        }

        /* get glyph */
        prev_glyph = glyph;
        dummy.code = code;
        glyph = av_tree_find(dtext->glyphs, &dummy, glyph_cmp, NULL);

        /* kerning */
        if (dtext->use_kerning && prev_glyph && glyph->code) {
            FT_Get_Kerning(dtext->face, prev_glyph->code, glyph->code,
                           ft_kerning_default, &delta);
            x += delta.x >> 6;
        }

        /* save position */
        dtext->positions[i].x = x + glyph->bitmap_left;
        dtext->positions[i].y = y - glyph->bitmap_top + y_max;
        if (code == '\t') x  = (x / dtext->tabsize + 1)*dtext->tabsize;
        else              x += glyph->advance;
    }

    max_text_line_w = FFMAX(x, max_text_line_w);

    dtext->var_values[VAR_TW] = dtext->var_values[VAR_TEXT_W] = max_text_line_w;
    dtext->var_values[VAR_TH] = dtext->var_values[VAR_TEXT_H] = y + dtext->max_glyph_h;

    dtext->var_values[VAR_MAX_GLYPH_W] = dtext->max_glyph_w;
    dtext->var_values[VAR_MAX_GLYPH_H] = dtext->max_glyph_h;
    dtext->var_values[VAR_MAX_GLYPH_A] = dtext->var_values[VAR_ASCENT ] = y_max;
    dtext->var_values[VAR_MAX_GLYPH_D] = dtext->var_values[VAR_DESCENT] = y_min;

    dtext->var_values[VAR_LINE_H] = dtext->var_values[VAR_LH] = dtext->max_glyph_h;

    dtext->x = dtext->var_values[VAR_X] = av_expr_eval(dtext->x_pexpr, dtext->var_values, &dtext->prng);
    dtext->y = dtext->var_values[VAR_Y] = av_expr_eval(dtext->y_pexpr, dtext->var_values, &dtext->prng);
    dtext->x = dtext->var_values[VAR_X] = av_expr_eval(dtext->x_pexpr, dtext->var_values, &dtext->prng);
    dtext->draw = av_expr_eval(dtext->draw_pexpr, dtext->var_values, &dtext->prng);

    if(!dtext->draw)
        return 0;

    box_w = FFMIN(width - 1 , max_text_line_w);
    box_h = FFMIN(height - 1, y + dtext->max_glyph_h);

    /* draw box */
    if (dtext->draw_box)
        ff_blend_rectangle(&dtext->dc, &dtext->boxcolor,
                           picref->data, picref->linesize, width, height,
                           dtext->x, dtext->y, box_w, box_h);

    if (dtext->shadowx || dtext->shadowy) {
        if ((ret = draw_glyphs(dtext, picref, width, height, dtext->shadowcolor.rgba,
                               &dtext->shadowcolor, dtext->shadowx, dtext->shadowy)) < 0)
            return ret;
    }

    if ((ret = draw_glyphs(dtext, picref, width, height, dtext->fontcolor.rgba,
                           &dtext->fontcolor, 0, 0)) < 0)
        return ret;

    return 0;
}

static void null_draw_slice(AVFilterLink *link, int y, int h, int slice_dir) { }

static void end_frame(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    AVFilterLink *outlink = ctx->outputs[0];
    DrawTextContext *dtext = ctx->priv;
    AVFilterBufferRef *picref = inlink->cur_buf;

    dtext->var_values[VAR_T] = picref->pts == AV_NOPTS_VALUE ?
        NAN : picref->pts * av_q2d(inlink->time_base);

    draw_text(ctx, picref, picref->video->w, picref->video->h);

    av_log(ctx, AV_LOG_DEBUG, "n:%d t:%f text_w:%d text_h:%d x:%d y:%d\n",
           (int)dtext->var_values[VAR_N], dtext->var_values[VAR_T],
           (int)dtext->var_values[VAR_TEXT_W], (int)dtext->var_values[VAR_TEXT_H],
           dtext->x, dtext->y);

    dtext->var_values[VAR_N] += 1.0;

    avfilter_draw_slice(outlink, 0, picref->video->h, 1);
    avfilter_end_frame(outlink);
}

AVFilter avfilter_vf_drawtext = {
    .name          = "drawtext",
    .description   = NULL_IF_CONFIG_SMALL("Draw text on top of video frames using libfreetype library."),
    .priv_size     = sizeof(DrawTextContext),
    .init          = init,
    .uninit        = uninit,
    .query_formats = query_formats,

    .inputs    = (const AVFilterPad[]) {{ .name       = "default",
                                    .type             = AVMEDIA_TYPE_VIDEO,
                                    .get_video_buffer = ff_null_get_video_buffer,
                                    .start_frame      = ff_null_start_frame,
                                    .draw_slice       = null_draw_slice,
                                    .end_frame        = end_frame,
                                    .config_props     = config_input,
                                    .min_perms        = AV_PERM_WRITE |
                                                        AV_PERM_READ,
                                    .rej_perms        = AV_PERM_PRESERVE },
                                  { .name = NULL}},
    .outputs   = (const AVFilterPad[]) {{ .name       = "default",
                                    .type             = AVMEDIA_TYPE_VIDEO, },
                                  { .name = NULL}},
    .process_command = command,
};
