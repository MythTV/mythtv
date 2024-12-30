/*
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
 * Intel Quick Sync Video VPP base function
 */

#include "libavutil/common.h"
#include "libavutil/mathematics.h"
#include "libavutil/mem.h"
#include "libavutil/time.h"
#include "libavutil/pixdesc.h"

#include "filters.h"
#include "qsvvpp.h"
#include "video.h"

#if QSV_ONEVPL
#include <mfxdispatcher.h>
#else
#define MFXUnload(a) do { } while(0)
#endif

#define IS_VIDEO_MEMORY(mode)  (mode & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | \
                                        MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
#if QSV_HAVE_OPAQUE
#define IS_OPAQUE_MEMORY(mode) (mode & MFX_MEMTYPE_OPAQUE_FRAME)
#endif
#define IS_SYSTEM_MEMORY(mode) (mode & MFX_MEMTYPE_SYSTEM_MEMORY)
#define MFX_IMPL_VIA_MASK(impl) (0x0f00 & (impl))

#define QSV_HAVE_AUDIO         !QSV_ONEVPL

static const AVRational default_tb = { 1, 90000 };

typedef struct QSVAsyncFrame {
    mfxSyncPoint  sync;
    QSVFrame     *frame;
} QSVAsyncFrame;

static const struct {
    int mfx_iopattern;
    const char *desc;
} qsv_iopatterns[] = {
    {MFX_IOPATTERN_IN_VIDEO_MEMORY,     "input is video memory surface"         },
    {MFX_IOPATTERN_IN_SYSTEM_MEMORY,    "input is system memory surface"        },
#if QSV_HAVE_OPAQUE
    {MFX_IOPATTERN_IN_OPAQUE_MEMORY,    "input is opaque memory surface"        },
#endif
    {MFX_IOPATTERN_OUT_VIDEO_MEMORY,    "output is video memory surface"        },
    {MFX_IOPATTERN_OUT_SYSTEM_MEMORY,   "output is system memory surface"       },
#if QSV_HAVE_OPAQUE
    {MFX_IOPATTERN_OUT_OPAQUE_MEMORY,   "output is opaque memory surface"       },
#endif
};

int ff_qsvvpp_print_iopattern(void *log_ctx, int mfx_iopattern,
                              const char *extra_string)
{
    const char *desc = NULL;

    for (int i = 0; i < FF_ARRAY_ELEMS(qsv_iopatterns); i++) {
        if (qsv_iopatterns[i].mfx_iopattern == mfx_iopattern) {
            desc = qsv_iopatterns[i].desc;
        }
    }
    if (!desc)
        desc = "unknown iopattern";

    av_log(log_ctx, AV_LOG_VERBOSE, "%s: %s\n", extra_string, desc);
    return 0;
}

static const struct {
    mfxStatus   mfxerr;
    int         averr;
    const char *desc;
} qsv_errors[] = {
    { MFX_ERR_NONE,                     0,               "success"                              },
    { MFX_ERR_UNKNOWN,                  AVERROR_UNKNOWN, "unknown error"                        },
    { MFX_ERR_NULL_PTR,                 AVERROR(EINVAL), "NULL pointer"                         },
    { MFX_ERR_UNSUPPORTED,              AVERROR(ENOSYS), "unsupported"                          },
    { MFX_ERR_MEMORY_ALLOC,             AVERROR(ENOMEM), "failed to allocate memory"            },
    { MFX_ERR_NOT_ENOUGH_BUFFER,        AVERROR(ENOMEM), "insufficient input/output buffer"     },
    { MFX_ERR_INVALID_HANDLE,           AVERROR(EINVAL), "invalid handle"                       },
    { MFX_ERR_LOCK_MEMORY,              AVERROR(EIO),    "failed to lock the memory block"      },
    { MFX_ERR_NOT_INITIALIZED,          AVERROR_BUG,     "not initialized"                      },
    { MFX_ERR_NOT_FOUND,                AVERROR(ENOSYS), "specified object was not found"       },
    /* the following 3 errors should always be handled explicitly, so those "mappings"
     * are for completeness only */
    { MFX_ERR_MORE_DATA,                AVERROR_UNKNOWN, "expect more data at input"            },
    { MFX_ERR_MORE_SURFACE,             AVERROR_UNKNOWN, "expect more surface at output"        },
    { MFX_ERR_MORE_BITSTREAM,           AVERROR_UNKNOWN, "expect more bitstream at output"      },
    { MFX_ERR_ABORTED,                  AVERROR_UNKNOWN, "operation aborted"                    },
    { MFX_ERR_DEVICE_LOST,              AVERROR(EIO),    "device lost"                          },
    { MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, AVERROR(EINVAL), "incompatible video parameters"        },
    { MFX_ERR_INVALID_VIDEO_PARAM,      AVERROR(EINVAL), "invalid video parameters"             },
    { MFX_ERR_UNDEFINED_BEHAVIOR,       AVERROR_BUG,     "undefined behavior"                   },
    { MFX_ERR_DEVICE_FAILED,            AVERROR(EIO),    "device failed"                        },
#if QSV_HAVE_AUDIO
    { MFX_ERR_INCOMPATIBLE_AUDIO_PARAM, AVERROR(EINVAL), "incompatible audio parameters"        },
    { MFX_ERR_INVALID_AUDIO_PARAM,      AVERROR(EINVAL), "invalid audio parameters"             },
#endif
    { MFX_ERR_GPU_HANG,                 AVERROR(EIO),    "GPU Hang"                             },
    { MFX_ERR_REALLOC_SURFACE,          AVERROR_UNKNOWN, "need bigger surface for output"       },

    { MFX_WRN_IN_EXECUTION,             0,               "operation in execution"               },
    { MFX_WRN_DEVICE_BUSY,              0,               "device busy"                          },
    { MFX_WRN_VIDEO_PARAM_CHANGED,      0,               "video parameters changed"             },
    { MFX_WRN_PARTIAL_ACCELERATION,     0,               "partial acceleration"                 },
    { MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0,               "incompatible video parameters"        },
    { MFX_WRN_VALUE_NOT_CHANGED,        0,               "value is saturated"                   },
    { MFX_WRN_OUT_OF_RANGE,             0,               "value out of range"                   },
    { MFX_WRN_FILTER_SKIPPED,           0,               "filter skipped"                       },
#if QSV_HAVE_AUDIO
    { MFX_WRN_INCOMPATIBLE_AUDIO_PARAM, 0,               "incompatible audio parameters"        },
#endif

#if QSV_VERSION_ATLEAST(1, 31)
    { MFX_ERR_NONE_PARTIAL_OUTPUT,      0,               "partial output"                       },
#endif
};

static int qsv_map_error(mfxStatus mfx_err, const char **desc)
{
    int i;
    for (i = 0; i < FF_ARRAY_ELEMS(qsv_errors); i++) {
        if (qsv_errors[i].mfxerr == mfx_err) {
            if (desc)
                *desc = qsv_errors[i].desc;
            return qsv_errors[i].averr;
        }
    }
    if (desc)
        *desc = "unknown error";
    return AVERROR_UNKNOWN;
}

int ff_qsvvpp_print_error(void *log_ctx, mfxStatus err,
                          const char *error_string)
{
    const char *desc;
    int ret;
    ret = qsv_map_error(err, &desc);
    av_log(log_ctx, AV_LOG_ERROR, "%s: %s (%d)\n", error_string, desc, err);
    return ret;
}

int ff_qsvvpp_print_warning(void *log_ctx, mfxStatus err,
                            const char *warning_string)
{
    const char *desc;
    int ret;
    ret = qsv_map_error(err, &desc);
    av_log(log_ctx, AV_LOG_WARNING, "%s: %s (%d)\n", warning_string, desc, err);
    return ret;
}

/* functions for frameAlloc */
static mfxStatus frame_alloc(mfxHDL pthis, mfxFrameAllocRequest *req,
                             mfxFrameAllocResponse *resp)
{
    QSVVPPContext *s = pthis;
    int i;

    if (!(req->Type & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET) ||
        !(req->Type & (MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_FROM_VPPOUT)) ||
        !(req->Type & MFX_MEMTYPE_EXTERNAL_FRAME))
        return MFX_ERR_UNSUPPORTED;

    if (req->Type & MFX_MEMTYPE_FROM_VPPIN) {
        resp->mids = av_mallocz(s->nb_surface_ptrs_in * sizeof(*resp->mids));
        if (!resp->mids)
            return AVERROR(ENOMEM);

        for (i = 0; i < s->nb_surface_ptrs_in; i++)
            resp->mids[i] = s->surface_ptrs_in[i]->Data.MemId;

        resp->NumFrameActual = s->nb_surface_ptrs_in;
    } else {
        resp->mids = av_mallocz(s->nb_surface_ptrs_out * sizeof(*resp->mids));
        if (!resp->mids)
            return AVERROR(ENOMEM);

        for (i = 0; i < s->nb_surface_ptrs_out; i++)
            resp->mids[i] = s->surface_ptrs_out[i]->Data.MemId;

        resp->NumFrameActual = s->nb_surface_ptrs_out;
    }

    return MFX_ERR_NONE;
}

static mfxStatus frame_free(mfxHDL pthis, mfxFrameAllocResponse *resp)
{
    av_freep(&resp->mids);
    return MFX_ERR_NONE;
}

static mfxStatus frame_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    return MFX_ERR_UNSUPPORTED;
}

static mfxStatus frame_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    return MFX_ERR_UNSUPPORTED;
}

static mfxStatus frame_get_hdl(mfxHDL pthis, mfxMemId mid, mfxHDL *hdl)
{
    mfxHDLPair *pair_dst = (mfxHDLPair*)hdl;
    mfxHDLPair *pair_src = (mfxHDLPair*)mid;

    pair_dst->first = pair_src->first;

    if (pair_src->second != (mfxMemId)MFX_INFINITE)
        pair_dst->second = pair_src->second;
    return MFX_ERR_NONE;
}

static int pix_fmt_to_mfx_fourcc(int format)
{
    switch (format) {
    case AV_PIX_FMT_YUV420P:
        return MFX_FOURCC_YV12;
    case AV_PIX_FMT_NV12:
        return MFX_FOURCC_NV12;
    case AV_PIX_FMT_YUYV422:
        return MFX_FOURCC_YUY2;
    case AV_PIX_FMT_BGRA:
        return MFX_FOURCC_RGB4;
    case AV_PIX_FMT_P010:
        return MFX_FOURCC_P010;
#if CONFIG_VAAPI
    case AV_PIX_FMT_UYVY422:
        return MFX_FOURCC_UYVY;
#endif
    }

    return MFX_FOURCC_NV12;
}

static int map_frame_to_surface(AVFrame *frame, mfxFrameSurface1 *surface)
{
    switch (frame->format) {
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_P010:
        surface->Data.Y  = frame->data[0];
        surface->Data.UV = frame->data[1];
        break;
    case AV_PIX_FMT_YUV420P:
        surface->Data.Y = frame->data[0];
        surface->Data.U = frame->data[1];
        surface->Data.V = frame->data[2];
        break;
    case AV_PIX_FMT_YUYV422:
        surface->Data.Y = frame->data[0];
        surface->Data.U = frame->data[0] + 1;
        surface->Data.V = frame->data[0] + 3;
        break;
    case AV_PIX_FMT_RGB32:
        surface->Data.B = frame->data[0];
        surface->Data.G = frame->data[0] + 1;
        surface->Data.R = frame->data[0] + 2;
        surface->Data.A = frame->data[0] + 3;
        break;
    case AV_PIX_FMT_UYVY422:
        surface->Data.Y = frame->data[0] + 1;
        surface->Data.U = frame->data[0];
        surface->Data.V = frame->data[0] + 2;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }
    surface->Data.Pitch = frame->linesize[0];

    return 0;
}

/* fill the surface info */
static int fill_frameinfo_by_link(mfxFrameInfo *frameinfo, AVFilterLink *link)
{
    FilterLink *l = ff_filter_link(link);
    enum AVPixelFormat        pix_fmt;
    AVHWFramesContext        *frames_ctx;
    AVQSVFramesContext       *frames_hwctx;
    const AVPixFmtDescriptor *desc;

    if (link->format == AV_PIX_FMT_QSV) {
        if (!l->hw_frames_ctx)
            return AVERROR(EINVAL);

        frames_ctx   = (AVHWFramesContext *)l->hw_frames_ctx->data;
        frames_hwctx = frames_ctx->hwctx;
        *frameinfo   = frames_hwctx->nb_surfaces ? frames_hwctx->surfaces[0].Info : *frames_hwctx->info;
    } else {
        pix_fmt = link->format;
        desc = av_pix_fmt_desc_get(pix_fmt);
        if (!desc)
            return AVERROR_BUG;

        frameinfo->CropX          = 0;
        frameinfo->CropY          = 0;
        frameinfo->Width          = FFALIGN(link->w, 32);
        frameinfo->Height         = FFALIGN(link->h, 32);
        frameinfo->PicStruct      = MFX_PICSTRUCT_PROGRESSIVE;
        frameinfo->FourCC         = pix_fmt_to_mfx_fourcc(pix_fmt);
        frameinfo->BitDepthLuma   = desc->comp[0].depth;
        frameinfo->BitDepthChroma = desc->comp[0].depth;
        frameinfo->Shift          = desc->comp[0].depth > 8;
        if (desc->log2_chroma_w && desc->log2_chroma_h)
            frameinfo->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        else if (desc->log2_chroma_w)
            frameinfo->ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        else
            frameinfo->ChromaFormat = MFX_CHROMAFORMAT_YUV444;
    }

    frameinfo->CropW          = link->w;
    frameinfo->CropH          = link->h;
    frameinfo->FrameRateExtN  = l->frame_rate.num;
    frameinfo->FrameRateExtD  = l->frame_rate.den;

    /* Apparently VPP in the SDK requires the frame rate to be set to some value, otherwise
     * init will fail */
    if (frameinfo->FrameRateExtD == 0 || frameinfo->FrameRateExtN == 0) {
        frameinfo->FrameRateExtN = 25;
        frameinfo->FrameRateExtD = 1;
    }

    frameinfo->AspectRatioW   = link->sample_aspect_ratio.num ? link->sample_aspect_ratio.num : 1;
    frameinfo->AspectRatioH   = link->sample_aspect_ratio.den ? link->sample_aspect_ratio.den : 1;

    return 0;
}

static void clear_unused_frames(QSVFrame *list)
{
    while (list) {
        /* list->queued==1 means the frame is not cached in VPP
         * process any more, it can be released to pool. */
        if ((list->queued == 1) && !list->surface.Data.Locked) {
            av_frame_free(&list->frame);
            list->queued = 0;
        }
        list = list->next;
    }
}

static void clear_frame_list(QSVFrame **list)
{
    while (*list) {
        QSVFrame *frame;

        frame = *list;
        *list = (*list)->next;
        av_frame_free(&frame->frame);
        av_freep(&frame);
    }
}

static QSVFrame *get_free_frame(QSVFrame **list)
{
    QSVFrame *out = *list;

    for (; out; out = out->next) {
        if (!out->queued) {
            out->queued = 1;
            break;
        }
    }

    if (!out) {
        out = av_mallocz(sizeof(*out));
        if (!out) {
            av_log(NULL, AV_LOG_ERROR, "Can't alloc new output frame.\n");
            return NULL;
        }
        out->queued = 1;
        out->next   = *list;
        *list       = out;
    }

    return out;
}

/* get the input surface */
static QSVFrame *submit_frame(QSVVPPContext *s, AVFilterLink *inlink, AVFrame *picref)
{
    QSVFrame        *qsv_frame;
    AVFilterContext *ctx = inlink->dst;

    clear_unused_frames(s->in_frame_list);

    qsv_frame = get_free_frame(&s->in_frame_list);
    if (!qsv_frame)
        return NULL;

    /* Turn AVFrame into mfxFrameSurface1.
     * For video/opaque memory mode, pix_fmt is AV_PIX_FMT_QSV, and
     * mfxFrameSurface1 is stored in AVFrame->data[3];
     * for system memory mode, raw video data is stored in
     * AVFrame, we should map it into mfxFrameSurface1.
     */
    if (!IS_SYSTEM_MEMORY(s->in_mem_mode)) {
        if (picref->format != AV_PIX_FMT_QSV) {
            av_log(ctx, AV_LOG_ERROR, "QSVVPP gets a wrong frame.\n");
            return NULL;
        }
        qsv_frame->frame   = av_frame_clone(picref);
        qsv_frame->surface = *(mfxFrameSurface1 *)qsv_frame->frame->data[3];
    } else {
        /* make a copy if the input is not padded as libmfx requires */
        if (picref->height & 31 || picref->linesize[0] & 31) {
            qsv_frame->frame = ff_get_video_buffer(inlink,
                                                   FFALIGN(inlink->w, 32),
                                                   FFALIGN(inlink->h, 32));
            if (!qsv_frame->frame)
                return NULL;

            qsv_frame->frame->width   = picref->width;
            qsv_frame->frame->height  = picref->height;

            if (av_frame_copy(qsv_frame->frame, picref) < 0) {
                av_frame_free(&qsv_frame->frame);
                return NULL;
            }
        } else
            qsv_frame->frame = av_frame_clone(picref);

        if (map_frame_to_surface(qsv_frame->frame,
                                 &qsv_frame->surface) < 0) {
            av_log(ctx, AV_LOG_ERROR, "Unsupported frame.\n");
            return NULL;
        }
    }

    qsv_frame->surface.Info           = s->frame_infos[FF_INLINK_IDX(inlink)];
    qsv_frame->surface.Data.TimeStamp = av_rescale_q(qsv_frame->frame->pts,
                                                      inlink->time_base, default_tb);

    qsv_frame->surface.Info.PicStruct =
            !(qsv_frame->frame->flags & AV_FRAME_FLAG_INTERLACED) ? MFX_PICSTRUCT_PROGRESSIVE :
            ((qsv_frame->frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) ? MFX_PICSTRUCT_FIELD_TFF :
                                                 MFX_PICSTRUCT_FIELD_BFF);
    if (qsv_frame->frame->repeat_pict == 1)
        qsv_frame->surface.Info.PicStruct |= MFX_PICSTRUCT_FIELD_REPEATED;
    else if (qsv_frame->frame->repeat_pict == 2)
        qsv_frame->surface.Info.PicStruct |= MFX_PICSTRUCT_FRAME_DOUBLING;
    else if (qsv_frame->frame->repeat_pict == 4)
        qsv_frame->surface.Info.PicStruct |= MFX_PICSTRUCT_FRAME_TRIPLING;

    return qsv_frame;
}

/* get the output surface */
static QSVFrame *query_frame(QSVVPPContext *s, AVFilterLink *outlink, const AVFrame *in)
{
    FilterLink *l = ff_filter_link(outlink);
    AVFilterContext *ctx = outlink->src;
    QSVFrame        *out_frame;
    int              ret;

    clear_unused_frames(s->out_frame_list);

    out_frame = get_free_frame(&s->out_frame_list);
    if (!out_frame)
        return NULL;

    /* For video memory, get a hw frame;
     * For system memory, get a sw frame and map it into a mfx_surface. */
    if (!IS_SYSTEM_MEMORY(s->out_mem_mode)) {
        out_frame->frame = av_frame_alloc();
        if (!out_frame->frame)
            return NULL;

        ret = av_hwframe_get_buffer(l->hw_frames_ctx, out_frame->frame, 0);
        if (ret < 0) {
            av_log(ctx, AV_LOG_ERROR, "Can't allocate a surface.\n");
            return NULL;
        }

        out_frame->surface = *(mfxFrameSurface1 *)out_frame->frame->data[3];
    } else {
        /* Get a frame with aligned dimensions.
         * Libmfx need system memory being 128x64 aligned */
        out_frame->frame = ff_get_video_buffer(outlink,
                                               FFALIGN(outlink->w, 128),
                                               FFALIGN(outlink->h, 64));
        if (!out_frame->frame)
            return NULL;

        ret = map_frame_to_surface(out_frame->frame,
                                   &out_frame->surface);
        if (ret < 0)
            return NULL;
    }

    if (l->frame_rate.num && l->frame_rate.den)
        out_frame->frame->duration = av_rescale_q(1, av_inv_q(l->frame_rate), outlink->time_base);
    else
        out_frame->frame->duration = 0;

    out_frame->frame->width  = outlink->w;
    out_frame->frame->height = outlink->h;
    out_frame->surface.Info = s->vpp_param.vpp.Out;

    for (int i = 0; i < s->vpp_param.NumExtParam; i++) {
        mfxExtBuffer *extbuf = s->vpp_param.ExtParam[i];

        if (extbuf->BufferId == MFX_EXTBUFF_VPP_DEINTERLACING) {
#if FF_API_INTERLACED_FRAME
FF_DISABLE_DEPRECATION_WARNINGS
            out_frame->frame->interlaced_frame = 0;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
            out_frame->frame->flags &= ~AV_FRAME_FLAG_INTERLACED;
            break;
        }
    }

    out_frame->surface.Info.PicStruct =
        !(out_frame->frame->flags & AV_FRAME_FLAG_INTERLACED) ? MFX_PICSTRUCT_PROGRESSIVE :
        ((out_frame->frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) ? MFX_PICSTRUCT_FIELD_TFF :
         MFX_PICSTRUCT_FIELD_BFF);

    return out_frame;
}

/* create the QSV session */
static int init_vpp_session(AVFilterContext *avctx, QSVVPPContext *s)
{
    AVFilterLink                 *inlink = avctx->inputs[0];
    FilterLink                      *inl = ff_filter_link(inlink);
    AVFilterLink                *outlink = avctx->outputs[0];
    FilterLink                     *outl = ff_filter_link(outlink);
    AVQSVFramesContext  *in_frames_hwctx = NULL;
    AVQSVFramesContext *out_frames_hwctx = NULL;

    AVBufferRef *device_ref;
    AVHWDeviceContext *device_ctx;
    AVQSVDeviceContext *device_hwctx;
    mfxHDL handle;
    mfxHandleType handle_type;
    mfxVersion ver;
    mfxIMPL impl;
    int ret, i;

    if (inl->hw_frames_ctx) {
        AVHWFramesContext *frames_ctx = (AVHWFramesContext *)inl->hw_frames_ctx->data;

        device_ref      = frames_ctx->device_ref;
        in_frames_hwctx = frames_ctx->hwctx;

        s->in_mem_mode = in_frames_hwctx->frame_type;

        s->surface_ptrs_in = av_calloc(in_frames_hwctx->nb_surfaces,
                                       sizeof(*s->surface_ptrs_in));
        if (!s->surface_ptrs_in)
            return AVERROR(ENOMEM);

        for (i = 0; i < in_frames_hwctx->nb_surfaces; i++)
            s->surface_ptrs_in[i] = in_frames_hwctx->surfaces + i;

        s->nb_surface_ptrs_in = in_frames_hwctx->nb_surfaces;
    } else if (avctx->hw_device_ctx) {
        device_ref     = avctx->hw_device_ctx;
        s->in_mem_mode = MFX_MEMTYPE_SYSTEM_MEMORY;
    } else {
        av_log(avctx, AV_LOG_ERROR, "No hw context provided.\n");
        return AVERROR(EINVAL);
    }

    device_ctx   = (AVHWDeviceContext *)device_ref->data;
    device_hwctx = device_ctx->hwctx;

    /* extract the properties of the "master" session given to us */
    ret = MFXQueryIMPL(device_hwctx->session, &impl);
    if (ret == MFX_ERR_NONE)
        ret = MFXQueryVersion(device_hwctx->session, &ver);
    if (ret != MFX_ERR_NONE) {
        av_log(avctx, AV_LOG_ERROR, "Error querying the session attributes\n");
        return AVERROR_UNKNOWN;
    }

    if (MFX_IMPL_VIA_VAAPI == MFX_IMPL_VIA_MASK(impl)) {
        handle_type = MFX_HANDLE_VA_DISPLAY;
    } else if (MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(impl)) {
        handle_type = MFX_HANDLE_D3D11_DEVICE;
    } else if (MFX_IMPL_VIA_D3D9 == MFX_IMPL_VIA_MASK(impl)) {
        handle_type = MFX_HANDLE_D3D9_DEVICE_MANAGER;
    } else {
        av_log(avctx, AV_LOG_ERROR, "Error unsupported handle type\n");
        return AVERROR_UNKNOWN;
    }

    if (outlink->format == AV_PIX_FMT_QSV) {
        AVHWFramesContext *out_frames_ctx;
        AVBufferRef *out_frames_ref = av_hwframe_ctx_alloc(device_ref);
        if (!out_frames_ref)
            return AVERROR(ENOMEM);

#if QSV_HAVE_OPAQUE
        s->out_mem_mode = IS_OPAQUE_MEMORY(s->in_mem_mode) ?
                          MFX_MEMTYPE_OPAQUE_FRAME :
                          MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_FROM_VPPOUT;
#else
        s->out_mem_mode = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_FROM_VPPOUT;
#endif

        out_frames_ctx   = (AVHWFramesContext *)out_frames_ref->data;
        out_frames_hwctx = out_frames_ctx->hwctx;

        out_frames_ctx->format            = AV_PIX_FMT_QSV;
        out_frames_ctx->width             = FFALIGN(outlink->w, 32);
        out_frames_ctx->height            = FFALIGN(outlink->h, 32);
        out_frames_ctx->sw_format         = s->out_sw_format;

        if (QSV_RUNTIME_VERSION_ATLEAST(ver, 2, 9) && handle_type != MFX_HANDLE_D3D9_DEVICE_MANAGER)
            out_frames_ctx->initial_pool_size = 0;
        else {
            out_frames_ctx->initial_pool_size = 64;
            if (avctx->extra_hw_frames > 0)
                out_frames_ctx->initial_pool_size += avctx->extra_hw_frames;
        }

        out_frames_hwctx->frame_type      = s->out_mem_mode;

        ret = av_hwframe_ctx_init(out_frames_ref);
        if (ret < 0) {
            av_buffer_unref(&out_frames_ref);
            av_log(avctx, AV_LOG_ERROR, "Error creating frames_ctx for output pad.\n");
            return ret;
        }

        s->surface_ptrs_out = av_calloc(out_frames_hwctx->nb_surfaces,
                                        sizeof(*s->surface_ptrs_out));
        if (!s->surface_ptrs_out) {
            av_buffer_unref(&out_frames_ref);
            return AVERROR(ENOMEM);
        }

        for (i = 0; i < out_frames_hwctx->nb_surfaces; i++)
            s->surface_ptrs_out[i] = out_frames_hwctx->surfaces + i;
        s->nb_surface_ptrs_out = out_frames_hwctx->nb_surfaces;

        av_buffer_unref(&outl->hw_frames_ctx);
        outl->hw_frames_ctx = out_frames_ref;
    } else
        s->out_mem_mode = MFX_MEMTYPE_SYSTEM_MEMORY;

    ret = MFXVideoCORE_GetHandle(device_hwctx->session, handle_type, &handle);
    if (ret < 0)
        return ff_qsvvpp_print_error(avctx, ret, "Error getting the session handle");
    else if (ret > 0) {
        ff_qsvvpp_print_warning(avctx, ret, "Warning in getting the session handle");
        return AVERROR_UNKNOWN;
    }

    /* create a "slave" session with those same properties, to be used for vpp */
    ret = ff_qsvvpp_create_mfx_session(avctx, device_hwctx->loader, impl, &ver,
                                       &s->session);
    if (ret)
        return ret;

    ret = MFXQueryVersion(s->session, &s->ver);
    if (ret != MFX_ERR_NONE) {
        av_log(avctx, AV_LOG_ERROR, "Error querying the runtime version\n");
        return AVERROR_UNKNOWN;
    }

    if (handle) {
        ret = MFXVideoCORE_SetHandle(s->session, handle_type, handle);
        if (ret != MFX_ERR_NONE)
            return AVERROR_UNKNOWN;
    }

    if (QSV_RUNTIME_VERSION_ATLEAST(ver, 1, 25)) {
        ret = MFXJoinSession(device_hwctx->session, s->session);
        if (ret != MFX_ERR_NONE)
            return AVERROR_UNKNOWN;
    }

#if QSV_HAVE_OPAQUE
    if (IS_OPAQUE_MEMORY(s->in_mem_mode) || IS_OPAQUE_MEMORY(s->out_mem_mode)) {
        s->opaque_alloc.In.Surfaces   = s->surface_ptrs_in;
        s->opaque_alloc.In.NumSurface = s->nb_surface_ptrs_in;
        s->opaque_alloc.In.Type       = s->in_mem_mode;

        s->opaque_alloc.Out.Surfaces   = s->surface_ptrs_out;
        s->opaque_alloc.Out.NumSurface = s->nb_surface_ptrs_out;
        s->opaque_alloc.Out.Type       = s->out_mem_mode;

        s->opaque_alloc.Header.BufferId = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION;
        s->opaque_alloc.Header.BufferSz = sizeof(s->opaque_alloc);
    } else
#endif
    if (IS_VIDEO_MEMORY(s->in_mem_mode) || IS_VIDEO_MEMORY(s->out_mem_mode)) {
        mfxFrameAllocator frame_allocator = {
            .pthis  = s,
            .Alloc  = frame_alloc,
            .Lock   = frame_lock,
            .Unlock = frame_unlock,
            .GetHDL = frame_get_hdl,
            .Free   = frame_free,
        };

        ret = MFXVideoCORE_SetFrameAllocator(s->session, &frame_allocator);
        if (ret != MFX_ERR_NONE)
            return AVERROR_UNKNOWN;
    }

    return 0;
}

static int set_frame_ext_params_null(AVFilterContext *ctx, const AVFrame *in, AVFrame *out, QSVVPPFrameParam *fp)
{
    return 0;
}

int ff_qsvvpp_init(AVFilterContext *avctx, QSVVPPParam *param)
{
    int i;
    int ret;
    QSVVPPContext *s = avctx->priv;

    s->filter_frame  = param->filter_frame;
    if (!s->filter_frame)
        s->filter_frame = ff_filter_frame;
    s->out_sw_format = param->out_sw_format;

    s->set_frame_ext_params = param->set_frame_ext_params;
    if (!s->set_frame_ext_params)
        s->set_frame_ext_params = set_frame_ext_params_null;

    /* create the vpp session */
    ret = init_vpp_session(avctx, s);
    if (ret < 0)
        goto failed;

    s->frame_infos = av_calloc(avctx->nb_inputs, sizeof(*s->frame_infos));
    if (!s->frame_infos) {
        ret = AVERROR(ENOMEM);
        goto failed;
    }

    /* Init each input's information */
    for (i = 0; i < avctx->nb_inputs; i++) {
        ret = fill_frameinfo_by_link(&s->frame_infos[i], avctx->inputs[i]);
        if (ret < 0)
            goto failed;
    }

    /* Update input's frame info according to crop */
    for (i = 0; i < param->num_crop; i++) {
        QSVVPPCrop *crop = param->crop + i;
        if (crop->in_idx > avctx->nb_inputs) {
            ret = AVERROR(EINVAL);
            goto failed;
        }
        s->frame_infos[crop->in_idx].CropX = crop->x;
        s->frame_infos[crop->in_idx].CropY = crop->y;
        s->frame_infos[crop->in_idx].CropW = crop->w;
        s->frame_infos[crop->in_idx].CropH = crop->h;
    }

    s->vpp_param.vpp.In = s->frame_infos[0];

    ret = fill_frameinfo_by_link(&s->vpp_param.vpp.Out, avctx->outputs[0]);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Fail to get frame info from link.\n");
        goto failed;
    }

    s->nb_seq_buffers = param->num_ext_buf;
#if QSV_HAVE_OPAQUE
    if (IS_OPAQUE_MEMORY(s->in_mem_mode) || IS_OPAQUE_MEMORY(s->out_mem_mode))
        s->nb_seq_buffers++;
#endif

    if (s->nb_seq_buffers) {
        s->seq_buffers = av_calloc(s->nb_seq_buffers, sizeof(*s->seq_buffers));
        if (!s->seq_buffers) {
            ret = AVERROR(ENOMEM);
            goto failed;
        }

        for (i = 0; i < param->num_ext_buf; i++)
            s->seq_buffers[i]    = param->ext_buf[i];

#if QSV_HAVE_OPAQUE
        if (IS_OPAQUE_MEMORY(s->in_mem_mode) || IS_OPAQUE_MEMORY(s->out_mem_mode))
            s->seq_buffers[i] = (mfxExtBuffer *)&s->opaque_alloc;
#endif

        s->nb_ext_buffers = s->nb_seq_buffers;
        s->ext_buffers = av_calloc(s->nb_ext_buffers, sizeof(*s->ext_buffers));
        if (!s->ext_buffers) {
            ret = AVERROR(ENOMEM);
            goto failed;
        }

        memcpy(s->ext_buffers, s->seq_buffers, s->nb_seq_buffers * sizeof(*s->seq_buffers));
    }

    s->vpp_param.ExtParam    = s->ext_buffers;
    s->vpp_param.NumExtParam = s->nb_ext_buffers;

    s->got_frame = 0;

    /** keep fifo size at least 1. Even when async_depth is 0, fifo is used. */
    s->async_fifo  = av_fifo_alloc2(s->async_depth + 1, sizeof(QSVAsyncFrame), 0);
    if (!s->async_fifo) {
        ret = AVERROR(ENOMEM);
        goto failed;
    }

    s->vpp_param.AsyncDepth = s->async_depth;

    if (IS_SYSTEM_MEMORY(s->in_mem_mode))
        s->vpp_param.IOPattern |= MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    else if (IS_VIDEO_MEMORY(s->in_mem_mode))
        s->vpp_param.IOPattern |= MFX_IOPATTERN_IN_VIDEO_MEMORY;
#if QSV_HAVE_OPAQUE
    else if (IS_OPAQUE_MEMORY(s->in_mem_mode))
        s->vpp_param.IOPattern |= MFX_IOPATTERN_IN_OPAQUE_MEMORY;
#endif

    if (IS_SYSTEM_MEMORY(s->out_mem_mode))
        s->vpp_param.IOPattern |= MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    else if (IS_VIDEO_MEMORY(s->out_mem_mode))
        s->vpp_param.IOPattern |= MFX_IOPATTERN_OUT_VIDEO_MEMORY;
#if QSV_HAVE_OPAQUE
    else if (IS_OPAQUE_MEMORY(s->out_mem_mode))
        s->vpp_param.IOPattern |= MFX_IOPATTERN_OUT_OPAQUE_MEMORY;
#endif

    /* Print input memory mode */
    ff_qsvvpp_print_iopattern(avctx, s->vpp_param.IOPattern & 0x0F, "VPP");
    /* Print output memory mode */
    ff_qsvvpp_print_iopattern(avctx, s->vpp_param.IOPattern & 0xF0, "VPP");

    /* Validate VPP params, but don't initial VPP session here */
    ret = MFXVideoVPP_Query(s->session, &s->vpp_param, &s->vpp_param);
    if (ret < 0) {
        ret = ff_qsvvpp_print_error(avctx, ret, "Error querying VPP params");
        goto failed;
    } else if (ret > 0)
        ff_qsvvpp_print_warning(avctx, ret, "Warning When querying VPP params");

    return 0;

failed:
    ff_qsvvpp_close(avctx);

    return ret;
}

static int qsvvpp_init_vpp_session(AVFilterContext *avctx, QSVVPPContext *s, const QSVFrame *in, QSVFrame *out)
{
    int ret;
    mfxExtBuffer *ext_param[QSVVPP_MAX_FRAME_EXTBUFS];
    QSVVPPFrameParam fp = { 0, ext_param };

    ret = s->set_frame_ext_params(avctx, in->frame, out->frame, &fp);
    if (ret)
        return ret;

    if (fp.num_ext_buf) {
        av_freep(&s->ext_buffers);
        s->nb_ext_buffers = s->nb_seq_buffers + fp.num_ext_buf;

        s->ext_buffers = av_calloc(s->nb_ext_buffers, sizeof(*s->ext_buffers));
        if (!s->ext_buffers)
            return AVERROR(ENOMEM);

        memcpy(&s->ext_buffers[0], s->seq_buffers, s->nb_seq_buffers * sizeof(*s->seq_buffers));
        memcpy(&s->ext_buffers[s->nb_seq_buffers], fp.ext_buf, fp.num_ext_buf * sizeof(*fp.ext_buf));
        s->vpp_param.ExtParam    = s->ext_buffers;
        s->vpp_param.NumExtParam = s->nb_ext_buffers;
    }

    if (!s->vpp_initted) {
        s->vpp_param.vpp.In.PicStruct = in->surface.Info.PicStruct;
        s->vpp_param.vpp.Out.PicStruct = out->surface.Info.PicStruct;

        /* Query VPP params again, including params for frame */
        ret = MFXVideoVPP_Query(s->session, &s->vpp_param, &s->vpp_param);
        if (ret < 0)
            return ff_qsvvpp_print_error(avctx, ret, "Error querying VPP params");
        else if (ret > 0)
            ff_qsvvpp_print_warning(avctx, ret, "Warning When querying VPP params");

        ret = MFXVideoVPP_Init(s->session, &s->vpp_param);
        if (ret < 0)
            return ff_qsvvpp_print_error(avctx, ret, "Failed to create a qsvvpp");
        else if (ret > 0)
            ff_qsvvpp_print_warning(avctx, ret, "Warning When creating qsvvpp");

        s->vpp_initted = 1;
    } else if (fp.num_ext_buf) {
        ret = MFXVideoVPP_Reset(s->session, &s->vpp_param);
        if (ret < 0) {
            ret = ff_qsvvpp_print_error(avctx, ret, "Failed to reset session for qsvvpp");
            return ret;
        } else if (ret > 0)
            ff_qsvvpp_print_warning(avctx, ret, "Warning When resetting session for qsvvpp");
    }

    return 0;
}

int ff_qsvvpp_close(AVFilterContext *avctx)
{
    QSVVPPContext *s = avctx->priv;

    if (s->session) {
        MFXVideoVPP_Close(s->session);
        MFXClose(s->session);
        s->session = NULL;
        s->vpp_initted = 0;
    }

    /* release all the resources */
    clear_frame_list(&s->in_frame_list);
    clear_frame_list(&s->out_frame_list);
    av_freep(&s->surface_ptrs_in);
    av_freep(&s->surface_ptrs_out);
    av_freep(&s->seq_buffers);
    av_freep(&s->ext_buffers);
    av_freep(&s->frame_infos);
    av_fifo_freep2(&s->async_fifo);

    return 0;
}

int ff_qsvvpp_filter_frame(QSVVPPContext *s, AVFilterLink *inlink, AVFrame *picref, AVFrame *propref)
{
    AVFilterContext  *ctx     = inlink->dst;
    AVFilterLink     *outlink = ctx->outputs[0];
    QSVAsyncFrame     aframe;
    mfxSyncPoint      sync;
    QSVFrame         *in_frame, *out_frame;
    int               ret, ret1, filter_ret;

    while (s->eof && av_fifo_read(s->async_fifo, &aframe, 1) >= 0) {
        if (MFXVideoCORE_SyncOperation(s->session, aframe.sync, 1000) < 0)
            av_log(ctx, AV_LOG_WARNING, "Sync failed.\n");

        filter_ret = s->filter_frame(outlink, aframe.frame->frame);
        if (filter_ret < 0) {
            av_frame_free(&aframe.frame->frame);
            return filter_ret;
        }
        aframe.frame->queued--;
        s->got_frame = 1;
        aframe.frame->frame = NULL;
    };

    if (!picref)
        return 0;

    in_frame = submit_frame(s, inlink, picref);
    if (!in_frame) {
        av_log(ctx, AV_LOG_ERROR, "Failed to submit frame on input[%d]\n",
               FF_INLINK_IDX(inlink));
        return AVERROR(ENOMEM);
    }

    do {
        out_frame = query_frame(s, outlink, in_frame->frame);
        if (!out_frame) {
            av_log(ctx, AV_LOG_ERROR, "Failed to query an output frame.\n");
            return AVERROR(ENOMEM);
        }

        ret = qsvvpp_init_vpp_session(ctx, s, in_frame, out_frame);
        if (ret)
            return ret;

        do {
            ret = MFXVideoVPP_RunFrameVPPAsync(s->session, &in_frame->surface,
                                               &out_frame->surface, NULL, &sync);
            if (ret == MFX_WRN_DEVICE_BUSY)
                av_usleep(500);
        } while (ret == MFX_WRN_DEVICE_BUSY);

        if (ret < 0 && ret != MFX_ERR_MORE_SURFACE) {
            /* Ignore more_data error */
            if (ret == MFX_ERR_MORE_DATA)
                return AVERROR(EAGAIN);
            break;
        }

        if (propref) {
            ret1 = av_frame_copy_props(out_frame->frame, propref);
            if (ret1 < 0) {
                av_frame_free(&out_frame->frame);
                av_log(ctx, AV_LOG_ERROR, "Failed to copy metadata fields from src to dst.\n");
                return ret1;
            }
        }

        out_frame->frame->pts = av_rescale_q(out_frame->surface.Data.TimeStamp,
                                             default_tb, outlink->time_base);

        out_frame->queued++;
        aframe = (QSVAsyncFrame){ sync, out_frame };
        av_fifo_write(s->async_fifo, &aframe, 1);

        if (av_fifo_can_read(s->async_fifo) > s->async_depth) {
            av_fifo_read(s->async_fifo, &aframe, 1);

            do {
                ret1 = MFXVideoCORE_SyncOperation(s->session, aframe.sync, 1000);
            } while (ret1 == MFX_WRN_IN_EXECUTION);

            if (ret1 < 0) {
                ret = ret1;
                break;
            }

            filter_ret = s->filter_frame(outlink, aframe.frame->frame);
            if (filter_ret < 0) {
                av_frame_free(&aframe.frame->frame);
                return filter_ret;
            }

            aframe.frame->queued--;
            s->got_frame = 1;
            aframe.frame->frame = NULL;
        }
    } while(ret == MFX_ERR_MORE_SURFACE);

    if (ret < 0)
        return ff_qsvvpp_print_error(ctx, ret, "Error running VPP");
    else if (ret > 0)
        ff_qsvvpp_print_warning(ctx, ret, "Warning in running VPP");

    return 0;
}

#if QSV_ONEVPL

int ff_qsvvpp_create_mfx_session(void *ctx,
                                 void *loader,
                                 mfxIMPL implementation,
                                 mfxVersion *pver,
                                 mfxSession *psession)
{
    mfxStatus sts;
    mfxSession session = NULL;
    uint32_t impl_idx = 0;

    av_log(ctx, AV_LOG_VERBOSE,
           "Use Intel(R) oneVPL to create MFX session with the specified MFX loader\n");

    if (!loader) {
        av_log(ctx, AV_LOG_ERROR, "Invalid MFX Loader handle\n");
        return AVERROR(EINVAL);
    }

    while (1) {
        /* Enumerate all implementations */
        mfxImplDescription *impl_desc;

        sts = MFXEnumImplementations(loader, impl_idx,
                                     MFX_IMPLCAPS_IMPLDESCSTRUCTURE,
                                     (mfxHDL *)&impl_desc);
        /* Failed to find an available implementation */
        if (sts == MFX_ERR_NOT_FOUND)
            break;
        else if (sts != MFX_ERR_NONE) {
            impl_idx++;
            continue;
        }

        sts = MFXCreateSession(loader, impl_idx, &session);
        MFXDispReleaseImplDescription(loader, impl_desc);
        if (sts == MFX_ERR_NONE)
            break;

        impl_idx++;
    }

    if (sts < 0)
        return ff_qsvvpp_print_error(ctx, sts,
                                     "Error creating a MFX session");

    *psession = session;

    return 0;
}

#else

int ff_qsvvpp_create_mfx_session(void *ctx,
                                 void *loader,
                                 mfxIMPL implementation,
                                 mfxVersion *pver,
                                 mfxSession *psession)
{
    mfxSession session = NULL;
    mfxStatus sts;

    av_log(ctx, AV_LOG_VERBOSE,
           "Use Intel(R) Media SDK to create MFX session, API version is "
           "%d.%d, the required implementation version is %d.%d\n",
           MFX_VERSION_MAJOR, MFX_VERSION_MINOR, pver->Major, pver->Minor);

    *psession = NULL;
    sts = MFXInit(implementation, pver, &session);
    if (sts < 0)
        return ff_qsvvpp_print_error(ctx, sts,
                                     "Error initializing an MFX session");
    else if (sts > 0) {
        ff_qsvvpp_print_warning(ctx, sts, "Warning in MFX session initialization");
        return AVERROR_UNKNOWN;
    }

    *psession = session;

    return 0;
}

#endif

AVFrame *ff_qsvvpp_get_video_buffer(AVFilterLink *inlink, int w, int h)
{
    /* When process YUV420 frames, FFmpeg uses same alignment on Y/U/V
     * planes. VPL and MSDK use Y plane's pitch / 2 as U/V planes's
     * pitch, which makes U/V planes 16-bytes aligned. We need to set a
     * separate alignment to meet runtime's behaviour.
    */
    return ff_default_get_video_buffer2(inlink,
                                        FFALIGN(inlink->w, 32),
                                        FFALIGN(inlink->h, 32),
                                        16);
}
