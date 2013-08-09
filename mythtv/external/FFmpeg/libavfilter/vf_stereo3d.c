/*
 * Copyright (c) 2010 Gordon Schmidt <gordon.schmidt <at> s2000.tu-chemnitz.de>
 * Copyright (c) 2013 Paul B Mahol
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"

enum StereoCode {
    ANAGLYPH_RC_GRAY,   // anaglyph red/cyan gray
    ANAGLYPH_RC_HALF,   // anaglyph red/cyan half colored
    ANAGLYPH_RC_COLOR,  // anaglyph red/cyan colored
    ANAGLYPH_RC_DUBOIS, // anaglyph red/cyan dubois
    ANAGLYPH_GM_GRAY,   // anaglyph green/magenta gray
    ANAGLYPH_GM_HALF,   // anaglyph green/magenta half colored
    ANAGLYPH_GM_COLOR,  // anaglyph green/magenta colored
    ANAGLYPH_GM_DUBOIS, // anaglyph green/magenta dubois
    ANAGLYPH_YB_GRAY,   // anaglyph yellow/blue gray
    ANAGLYPH_YB_HALF,   // anaglyph yellow/blue half colored
    ANAGLYPH_YB_COLOR,  // anaglyph yellow/blue colored
    ANAGLYPH_YB_DUBOIS, // anaglyph yellow/blue dubois
    ANAGLYPH_RB_GRAY,   // anaglyph red/blue gray
    ANAGLYPH_RG_GRAY,   // anaglyph red/green gray
    MONO_L,             // mono output for debugging (left eye only)
    MONO_R,             // mono output for debugging (right eye only)
    INTERLEAVE_ROWS_LR, // row-interleave (left eye has top row)
    INTERLEAVE_ROWS_RL, // row-interleave (right eye has top row)
    SIDE_BY_SIDE_LR,    // side by side parallel (left eye left, right eye right)
    SIDE_BY_SIDE_RL,    // side by side crosseye (right eye left, left eye right)
    SIDE_BY_SIDE_2_LR,  // side by side parallel with half width resolution
    SIDE_BY_SIDE_2_RL,  // side by side crosseye with half width resolution
    ABOVE_BELOW_LR,     // above-below (left eye above, right eye below)
    ABOVE_BELOW_RL,     // above-below (right eye above, left eye below)
    ABOVE_BELOW_2_LR,   // above-below with half height resolution
    ABOVE_BELOW_2_RL,   // above-below with half height resolution
    STEREO_CODE_COUNT   // TODO: needs autodetection
};

typedef struct StereoComponent {
    enum StereoCode format;
    int width, height;
    int off_left, off_right;
    int row_left, row_right;
} StereoComponent;

static const int ana_coeff[][3][6] = {
  [ANAGLYPH_RB_GRAY]   =
    {{19595, 38470,  7471,     0,     0,     0},
     {    0,     0,     0,     0,     0,     0},
     {    0,     0,     0, 19595, 38470,  7471}},
  [ANAGLYPH_RG_GRAY]   =
    {{19595, 38470,  7471,     0,     0,     0},
     {    0,     0,     0, 19595, 38470,  7471},
     {    0,     0,     0,     0,     0,     0}},
  [ANAGLYPH_RC_GRAY]   =
    {{19595, 38470,  7471,     0,     0,     0},
     {    0,     0,     0, 19595, 38470,  7471},
     {    0,     0,     0, 19595, 38470,  7471}},
  [ANAGLYPH_RC_HALF]   =
    {{19595, 38470,  7471,     0,     0,     0},
     {    0,     0,     0,     0, 65536,     0},
     {    0,     0,     0,     0,     0, 65536}},
  [ANAGLYPH_RC_COLOR]  =
    {{65536,     0,     0,     0,     0,     0},
     {    0,     0,     0,     0, 65536,     0},
     {    0,     0,     0,     0,     0, 65536}},
  [ANAGLYPH_RC_DUBOIS] =
    {{29891, 32800, 11559, -2849, -5763,  -102},
     {-2627, -2479, -1033, 24804, 48080, -1209},
     { -997, -1350,  -358, -4729, -7403, 80373}},
  [ANAGLYPH_GM_GRAY]   =
    {{    0,     0,     0, 19595, 38470,  7471},
     {19595, 38470,  7471,     0,     0,     0},
     {    0,     0,     0, 19595, 38470,  7471}},
  [ANAGLYPH_GM_HALF]   =
    {{    0,     0,     0, 65536,     0,     0},
     {19595, 38470,  7471,     0,     0,     0},
     {    0,     0,     0,     0,     0, 65536}},
  [ANAGLYPH_GM_COLOR]  =
    {{    0,     0,     0, 65536,     0,     0},
     {    0, 65536,     0,     0,     0,     0},
     {    0,     0,     0,     0,     0, 65536}},
  [ANAGLYPH_GM_DUBOIS]  =
    {{-4063,-10354, -2556, 34669, 46203,  1573},
     {18612, 43778,  9372, -1049,  -983, -4260},
     { -983, -1769,  1376,   590,  4915, 61407}},
  [ANAGLYPH_YB_GRAY]   =
    {{    0,     0,     0, 19595, 38470,  7471},
     {    0,     0,     0, 19595, 38470,  7471},
     {19595, 38470,  7471,     0,     0,     0}},
  [ANAGLYPH_YB_HALF]   =
    {{    0,     0,     0, 65536,     0,     0},
     {    0,     0,     0,     0, 65536,     0},
     {19595, 38470,  7471,     0,     0,     0}},
  [ANAGLYPH_YB_COLOR]  =
    {{    0,     0,     0, 65536,     0,     0},
     {    0,     0,     0,     0, 65536,     0},
     {    0,     0, 65536,     0,     0,     0}},
  [ANAGLYPH_YB_DUBOIS] =
    {{65535,-12650,18451,   -987, -7590, -1049},
     {-1604, 56032, 4196,    370,  3826, -1049},
     {-2345,-10676, 1358,   5801, 11416, 56217}},
};

typedef struct Stereo3DContext {
    const AVClass *class;
    StereoComponent in, out;
    int width, height;
    int row_step;
    int ana_matrix[3][6];
} Stereo3DContext;

#define OFFSET(x) offsetof(Stereo3DContext, x)
#define FLAGS AV_OPT_FLAG_FILTERING_PARAM|AV_OPT_FLAG_VIDEO_PARAM

static const AVOption stereo3d_options[] = {
    { "in",    "set input format",  OFFSET(in.format),  AV_OPT_TYPE_INT, {.i64=SIDE_BY_SIDE_LR}, SIDE_BY_SIDE_LR, ABOVE_BELOW_2_RL, FLAGS, "in"},
    { "ab2l",  "above below half height left first",  0, AV_OPT_TYPE_CONST, {.i64=ABOVE_BELOW_2_LR},  0, 0, FLAGS, "in" },
    { "ab2r",  "above below half height right first", 0, AV_OPT_TYPE_CONST, {.i64=ABOVE_BELOW_2_RL},  0, 0, FLAGS, "in" },
    { "abl",   "above below left first",              0, AV_OPT_TYPE_CONST, {.i64=ABOVE_BELOW_LR},    0, 0, FLAGS, "in" },
    { "abr",   "above below right first",             0, AV_OPT_TYPE_CONST, {.i64=ABOVE_BELOW_RL},    0, 0, FLAGS, "in" },
    { "sbs2l", "side by side half width left first",  0, AV_OPT_TYPE_CONST, {.i64=SIDE_BY_SIDE_2_LR}, 0, 0, FLAGS, "in" },
    { "sbs2r", "side by side half width right first", 0, AV_OPT_TYPE_CONST, {.i64=SIDE_BY_SIDE_2_RL}, 0, 0, FLAGS, "in" },
    { "sbsl",  "side by side left first",             0, AV_OPT_TYPE_CONST, {.i64=SIDE_BY_SIDE_LR},   0, 0, FLAGS, "in" },
    { "sbsr",  "side by side right first",            0, AV_OPT_TYPE_CONST, {.i64=SIDE_BY_SIDE_RL},   0, 0, FLAGS, "in" },
    { "out",   "set output format", OFFSET(out.format), AV_OPT_TYPE_INT, {.i64=ANAGLYPH_RC_DUBOIS}, 0, STEREO_CODE_COUNT-1, FLAGS, "out"},
    { "ab2l",  "above below half height left first",  0, AV_OPT_TYPE_CONST, {.i64=ABOVE_BELOW_2_LR},   0, 0, FLAGS, "out" },
    { "ab2r",  "above below half height right first", 0, AV_OPT_TYPE_CONST, {.i64=ABOVE_BELOW_2_RL},   0, 0, FLAGS, "out" },
    { "abl",   "above below left first",              0, AV_OPT_TYPE_CONST, {.i64=ABOVE_BELOW_LR},     0, 0, FLAGS, "out" },
    { "abr",   "above below right first",             0, AV_OPT_TYPE_CONST, {.i64=ABOVE_BELOW_RL},     0, 0, FLAGS, "out" },
    { "agmc",  "anaglyph green magenta color",        0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_GM_COLOR},  0, 0, FLAGS, "out" },
    { "agmd",  "anaglyph green magenta dubois",       0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_GM_DUBOIS}, 0, 0, FLAGS, "out" },
    { "agmg",  "anaglyph green magenta gray",         0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_GM_GRAY},   0, 0, FLAGS, "out" },
    { "agmh",  "anaglyph green magenta half color",   0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_GM_HALF},   0, 0, FLAGS, "out" },
    { "arbg",  "anaglyph red blue gray",              0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_RB_GRAY},   0, 0, FLAGS, "out" },
    { "arcc",  "anaglyph red cyan color",             0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_RC_COLOR},  0, 0, FLAGS, "out" },
    { "arcd",  "anaglyph red cyan dubois",            0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_RC_DUBOIS}, 0, 0, FLAGS, "out" },
    { "arcg",  "anaglyph red cyan gray",              0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_RC_GRAY},   0, 0, FLAGS, "out" },
    { "arch",  "anaglyph red cyan half color",        0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_RC_HALF},   0, 0, FLAGS, "out" },
    { "argg",  "anaglyph red green gray",             0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_RG_GRAY},   0, 0, FLAGS, "out" },
    { "aybc",  "anaglyph yellow blue color",          0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_YB_COLOR},  0, 0, FLAGS, "out" },
    { "aybd",  "anaglyph yellow blue dubois",         0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_YB_DUBOIS}, 0, 0, FLAGS, "out" },
    { "aybg",  "anaglyph yellow blue gray",           0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_YB_GRAY},   0, 0, FLAGS, "out" },
    { "aybh",  "anaglyph yellow blue half color",     0, AV_OPT_TYPE_CONST, {.i64=ANAGLYPH_YB_HALF},   0, 0, FLAGS, "out" },
    { "irl",   "interleave rows left first",          0, AV_OPT_TYPE_CONST, {.i64=INTERLEAVE_ROWS_LR}, 0, 0, FLAGS, "out" },
    { "irr",   "interleave rows right first",         0, AV_OPT_TYPE_CONST, {.i64=INTERLEAVE_ROWS_RL}, 0, 0, FLAGS, "out" },
    { "ml",    "mono left",                           0, AV_OPT_TYPE_CONST, {.i64=MONO_L},             0, 0, FLAGS, "out" },
    { "mr",    "mono right",                          0, AV_OPT_TYPE_CONST, {.i64=MONO_R},             0, 0, FLAGS, "out" },
    { "sbs2l", "side by side half width left first",  0, AV_OPT_TYPE_CONST, {.i64=SIDE_BY_SIDE_2_LR},  0, 0, FLAGS, "out" },
    { "sbs2r", "side by side half width right first", 0, AV_OPT_TYPE_CONST, {.i64=SIDE_BY_SIDE_2_RL},  0, 0, FLAGS, "out" },
    { "sbsl",  "side by side left first",             0, AV_OPT_TYPE_CONST, {.i64=SIDE_BY_SIDE_LR},    0, 0, FLAGS, "out" },
    { "sbsr",  "side by side right first",            0, AV_OPT_TYPE_CONST, {.i64=SIDE_BY_SIDE_RL},    0, 0, FLAGS, "out" },
    {NULL},
};

AVFILTER_DEFINE_CLASS(stereo3d);

static av_cold int init(AVFilterContext *ctx, const char *args)
{
    Stereo3DContext *s = ctx->priv;
    static const char *shorthand[] = { "in", "out", NULL };
    int ret;

    s->class = &stereo3d_class;
    av_opt_set_defaults(s);

    if ((ret = av_opt_set_from_string(s, args, shorthand, "=", ":")) < 0)
        return ret;

    return 0;
}

static int query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat pix_fmts[] = {
        AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE
    };

    ff_set_common_formats(ctx, ff_make_format_list(pix_fmts));

    return 0;
}

static int config_output(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    AVFilterLink *inlink = ctx->inputs[0];
    Stereo3DContext *s = ctx->priv;
    AVRational aspect = inlink->sample_aspect_ratio;

    s->in.width     =
    s->width        = inlink->w;
    s->in.height    =
    s->height       = inlink->h;
    s->row_step     = 1;
    s->in.off_left  =
    s->in.off_right =
    s->in.row_left  =
    s->in.row_right = 0;

    switch (s->in.format) {
    case SIDE_BY_SIDE_2_LR:
        aspect.num     *= 2;
    case SIDE_BY_SIDE_LR:
        s->width        = inlink->w / 2;
        s->in.off_right = s->width * 3;
        break;
    case SIDE_BY_SIDE_2_RL:
        aspect.num     *= 2;
    case SIDE_BY_SIDE_RL:
        s->width        = inlink->w / 2;
        s->in.off_left  = s->width * 3;
        break;
    case ABOVE_BELOW_2_LR:
        aspect.den     *= 2;
    case ABOVE_BELOW_LR:
        s->in.row_right =
        s->height       = inlink->h / 2;
        break;
    case ABOVE_BELOW_2_RL:
        aspect.den     *= 2;
    case ABOVE_BELOW_RL:
        s->in.row_left  =
        s->height       = inlink->h / 2;
        break;
    default:
        av_log(ctx, AV_LOG_ERROR, "input format %d is not supported\n", s->in.format);
        return AVERROR(EINVAL);
    }

    s->out.width     = s->width;
    s->out.height    = s->height;
    s->out.off_left  =
    s->out.off_right =
    s->out.row_left  =
    s->out.row_right = 0;

    switch (s->out.format) {
    case ANAGLYPH_RB_GRAY:
    case ANAGLYPH_RG_GRAY:
    case ANAGLYPH_RC_GRAY:
    case ANAGLYPH_RC_HALF:
    case ANAGLYPH_RC_COLOR:
    case ANAGLYPH_RC_DUBOIS:
    case ANAGLYPH_GM_GRAY:
    case ANAGLYPH_GM_HALF:
    case ANAGLYPH_GM_COLOR:
    case ANAGLYPH_GM_DUBOIS:
    case ANAGLYPH_YB_GRAY:
    case ANAGLYPH_YB_HALF:
    case ANAGLYPH_YB_COLOR:
    case ANAGLYPH_YB_DUBOIS:
        memcpy(s->ana_matrix, ana_coeff[s->out.format], sizeof(s->ana_matrix));
        break;
    case SIDE_BY_SIDE_2_LR:
        aspect.num      /= 2;
    case SIDE_BY_SIDE_LR:
        s->out.width     =
        s->out.off_right = s->width * 3;
        break;
    case SIDE_BY_SIDE_2_RL:
        aspect.num      /= 2;
    case SIDE_BY_SIDE_RL:
        s->out.width     = s->width * 2;
        s->out.off_left  = s->width * 3;
        break;
    case ABOVE_BELOW_2_LR:
        aspect.den      /= 2;
    case ABOVE_BELOW_LR:
        s->out.height    = s->height * 2;
        s->out.row_right = s->height;
        break;
    case ABOVE_BELOW_2_RL:
        aspect.den      /= 2;
    case ABOVE_BELOW_RL:
        s->out.height    = s->height * 2;
        s->out.row_left  = s->height;
        break;
    case INTERLEAVE_ROWS_LR:
        s->row_step      = 2;
        s->height        = s->height / 2;
        s->out.off_right = s->width * 3;
        s->in.off_right += s->in.width * 3;
        break;
    case INTERLEAVE_ROWS_RL:
        s->row_step      = 2;
        s->height        = s->height / 2;
        s->out.off_left  = s->width * 3;
        s->in.off_left  += s->in.width * 3;
        break;
    case MONO_R:
        s->in.off_left   = s->in.off_right;
        s->in.row_left   = s->in.row_right;
    case MONO_L:
        break;
    default:
        av_log(ctx, AV_LOG_ERROR, "output format is not supported\n");
        return AVERROR(EINVAL);
    }

    outlink->w = s->out.width;
    outlink->h = s->out.height;
    outlink->sample_aspect_ratio = aspect;

    return 0;
}

static inline uint8_t ana_convert(const int *coeff, uint8_t *left, uint8_t *right)
{
    int sum;

    sum  = coeff[0] * left[0] + coeff[3] * right[0]; //red in
    sum += coeff[1] * left[1] + coeff[4] * right[1]; //green in
    sum += coeff[2] * left[2] + coeff[5] * right[2]; //blue in

    return av_clip_uint8(sum >> 16);
}

static int filter_frame(AVFilterLink *inlink, AVFilterBufferRef *inpicref)
{
    AVFilterContext *ctx  = inlink->dst;
    Stereo3DContext *s = ctx->priv;
    AVFilterLink *outlink = ctx->outputs[0];
    AVFilterBufferRef *out;
    int out_off_left, out_off_right;
    int in_off_left, in_off_right;
    int ret;

    out = ff_get_video_buffer(outlink, AV_PERM_WRITE, outlink->w, outlink->h);
    if (!out) {
        avfilter_unref_bufferp(&inpicref);
        return AVERROR(ENOMEM);
    }

    out->pts = inpicref->pts;
    out->pos = inpicref->pos;

    in_off_left   = s->in.row_left  * inpicref->linesize[0] + s->in.off_left;
    in_off_right  = s->in.row_right * inpicref->linesize[0] + s->in.off_right;
    out_off_left  = s->out.row_left  * out->linesize[0] + s->out.off_left;
    out_off_right = s->out.row_right * out->linesize[0] + s->out.off_right;

    switch (s->out.format) {
    case SIDE_BY_SIDE_LR:
    case SIDE_BY_SIDE_RL:
    case SIDE_BY_SIDE_2_LR:
    case SIDE_BY_SIDE_2_RL:
    case ABOVE_BELOW_LR:
    case ABOVE_BELOW_RL:
    case ABOVE_BELOW_2_LR:
    case ABOVE_BELOW_2_RL:
    case INTERLEAVE_ROWS_LR:
    case INTERLEAVE_ROWS_RL:
        av_image_copy_plane(out->data[0] + out_off_left,
                            out->linesize[0] * s->row_step,
                            inpicref->data[0] + in_off_left,
                            inpicref->linesize[0] * s->row_step,
                            3 * s->width, s->height);
        av_image_copy_plane(out->data[0] + out_off_right,
                            out->linesize[0] * s->row_step,
                            inpicref->data[0] + in_off_right,
                            inpicref->linesize[0] * s->row_step,
                            3 * s->width, s->height);
        break;
    case MONO_L:
    case MONO_R:
        av_image_copy_plane(out->data[0], out->linesize[0],
                            inpicref->data[0] + in_off_left,
                            inpicref->linesize[0],
                            3 * s->width, s->height);
        break;
    case ANAGLYPH_RB_GRAY:
    case ANAGLYPH_RG_GRAY:
    case ANAGLYPH_RC_GRAY:
    case ANAGLYPH_RC_HALF:
    case ANAGLYPH_RC_COLOR:
    case ANAGLYPH_RC_DUBOIS:
    case ANAGLYPH_GM_GRAY:
    case ANAGLYPH_GM_HALF:
    case ANAGLYPH_GM_COLOR:
    case ANAGLYPH_GM_DUBOIS:
    case ANAGLYPH_YB_GRAY:
    case ANAGLYPH_YB_HALF:
    case ANAGLYPH_YB_COLOR:
    case ANAGLYPH_YB_DUBOIS: {
        int i, x, y, il, ir, o;
        uint8_t *src = inpicref->data[0];
        uint8_t *dst = out->data[0];
        int out_width = s->out.width;
        int *ana_matrix[3];

        for (i = 0; i < 3; i++)
            ana_matrix[i] = s->ana_matrix[i];

        for (y = 0; y < s->out.height; y++) {
            o   = out->linesize[0] * y;
            il  = in_off_left  + y * inpicref->linesize[0];
            ir  = in_off_right + y * inpicref->linesize[0];
            for (x = 0; x < out_width; x++, il += 3, ir += 3, o+= 3) {
                dst[o    ] = ana_convert(ana_matrix[0], src + il, src + ir);
                dst[o + 1] = ana_convert(ana_matrix[1], src + il, src + ir);
                dst[o + 2] = ana_convert(ana_matrix[2], src + il, src + ir);
            }
        }
        break;
    }
    default:
        av_assert0(0);
    }

    ret = ff_filter_frame(outlink, out);
    avfilter_unref_bufferp(&inpicref);
    if (ret < 0)
        return ret;
    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    Stereo3DContext *s = ctx->priv;

    av_opt_free(s);
}

static const AVFilterPad stereo3d_inputs[] = {
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_VIDEO,
        .get_video_buffer = ff_null_get_video_buffer,
        .filter_frame     = filter_frame,
        .min_perms        = AV_PERM_READ,
    },
    { NULL }
};

static const AVFilterPad stereo3d_outputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .config_props = config_output,
        .min_perms    = AV_PERM_WRITE,
    },
    { NULL }
};

AVFilter avfilter_vf_stereo3d = {
    .name          = "stereo3d",
    .description   = NULL_IF_CONFIG_SMALL("Convert video stereoscopic 3D view."),
    .priv_size     = sizeof(Stereo3DContext),
    .init          = init,
    .uninit        = uninit,
    .query_formats = query_formats,
    .inputs        = stereo3d_inputs,
    .outputs       = stereo3d_outputs,
    .priv_class    = &stereo3d_class,
};
