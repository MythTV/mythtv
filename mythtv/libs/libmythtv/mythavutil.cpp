//
//  mythavutil.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 28/06/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//

#include "mythframe.h"
#include "mythavutil.h"
#include "mythcorecontext.h"
#include "mythconfig.h"
extern "C" {
#include "libswscale/swscale.h"
#include "libavfilter/avfilter.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
}
#include <QMutexLocker>

AVPixelFormat FrameTypeToPixelFormat(VideoFrameType type)
{
    switch (type)
    {
        case FMT_NV12:
            return AV_PIX_FMT_NV12;
        case FMT_YUV422P:
            return AV_PIX_FMT_YUV422P;
        case FMT_BGRA:
            return AV_PIX_FMT_BGRA;
        case FMT_YUY2:
            return AV_PIX_FMT_UYVY422;
        case FMT_RGB24:
            return AV_PIX_FMT_RGB24;
        case FMT_RGB32:
            return AV_PIX_FMT_RGB32;
        default:
            return AV_PIX_FMT_YUV420P;
    }
}

VideoFrameType PixelFormatToFrameType(AVPixelFormat fmt)
{
    switch (fmt)
    {
        case AV_PIX_FMT_NV12:
            return FMT_NV12;
        case AV_PIX_FMT_YUV422P:
            return FMT_YUV422P;
        case AV_PIX_FMT_RGB32:
            return FMT_RGB32;
        case AV_PIX_FMT_UYVY422:
            return FMT_YUY2;
        case AV_PIX_FMT_RGB24:
            return FMT_RGB24;
        default:
            return FMT_YV12;
    }
}

int AVPictureFill(AVFrame *pic, const VideoFrame *frame, AVPixelFormat fmt)
{
    if (fmt == AV_PIX_FMT_NONE)
    {
        fmt = FrameTypeToPixelFormat(frame->codec);
    }

    av_image_fill_arrays(pic->data, pic->linesize, frame->buf,
        fmt, frame->width, frame->height, IMAGE_ALIGN);
    pic->data[1] = frame->buf + frame->offsets[1];
    pic->data[2] = frame->buf + frame->offsets[2];
    pic->linesize[0] = frame->pitches[0];
    pic->linesize[1] = frame->pitches[1];
    pic->linesize[2] = frame->pitches[2];
    return (int)buffersize(frame->codec, frame->width, frame->height);
}

class MythAVCopyPrivate
{
public:
    explicit MythAVCopyPrivate(bool uswc)
    : swsctx(NULL), copyctx(new MythUSWCCopy(4096, !uswc)),
      width(0), height(0), size(0), format(AV_PIX_FMT_NONE)
    {
    }

    ~MythAVCopyPrivate()
    {
        if (swsctx)
        {
            sws_freeContext(swsctx);
        }
        delete copyctx;
    }

    int SizeData(int _width, int _height, AVPixelFormat _fmt)
    {
        if (_width == width && _height == height && _fmt == format)
        {
            return size;
        }
        size    = av_image_get_buffer_size(_fmt, _width, _height, IMAGE_ALIGN);
        width   = _width;
        height  = _height;
        format  = _fmt;
        return size;
    }

    SwsContext *swsctx;
    MythUSWCCopy *copyctx;
    int width, height, size;
    AVPixelFormat format;
};

MythAVCopy::MythAVCopy(bool uswc) : d(new MythAVCopyPrivate(uswc))
{
}

MythAVCopy::~MythAVCopy()
{
    delete d;
}

void MythAVCopy::FillFrame(VideoFrame *frame, const AVFrame *pic, int pitch,
                           int width, int height, AVPixelFormat pix_fmt)
{
    int size = av_image_get_buffer_size(pix_fmt, width, height, IMAGE_ALIGN);

    if (pix_fmt == AV_PIX_FMT_YUV420P)
    {
        int chroma_pitch  = pitch >> 1;
        int chroma_height = height >> 1;
        int offsets[3] =
            { 0,
              pitch * height,
              pitch * height + chroma_pitch * chroma_height };
        int pitches[3] = { pitch, chroma_pitch, chroma_pitch };

        init(frame, FMT_YV12, pic->data[0], width, height, size, pitches, offsets);
    }
    else if (pix_fmt == AV_PIX_FMT_NV12)
    {
        int offsets[3] = { 0, pitch * height, 0 };
        int pitches[3] = { pitch, pitch, 0 };

        init(frame, FMT_NV12, pic->data[0], width, height, size, pitches, offsets);
    }
}

int MythAVCopy::Copy(AVFrame *dst, AVPixelFormat dst_pix_fmt,
                 const AVFrame *src, AVPixelFormat pix_fmt,
                 int width, int height)
{
    if ((pix_fmt == AV_PIX_FMT_YUV420P || pix_fmt == AV_PIX_FMT_NV12) &&
        (dst_pix_fmt == AV_PIX_FMT_YUV420P || dst_pix_fmt == AV_PIX_FMT_NV12))
    {
        VideoFrame framein, frameout;

        FillFrame(&framein, src, width, width, height, pix_fmt);
        FillFrame(&frameout, dst, width, width, height, dst_pix_fmt);

        d->copyctx->copy(&frameout, &framein);
        return frameout.size;
    }

    int new_width = width;
#if ARCH_ARM
    // The ARM build of FFMPEG has a bug that if sws_scale is
    // called with source and dest sizes the same, and
    // formats as shown below, it causes a bus error and the
    // application core dumps. To avoid this I make a -1
    // difference in the new width, causing it to bypass
    // the code optimization which is failing.
    if (pix_fmt == AV_PIX_FMT_YUV420P
      && dst_pix_fmt == AV_PIX_FMT_BGRA)
        new_width = width - 1;
#endif
    d->swsctx = sws_getCachedContext(d->swsctx, width, height, pix_fmt,
                                     new_width, height, dst_pix_fmt,
                                     SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (d->swsctx == NULL)
    {
        return -1;
    }

    sws_scale(d->swsctx, src->data, src->linesize,
              0, height, dst->data, dst->linesize);

    return d->SizeData(width, height, dst_pix_fmt);
}

int MythAVCopy::Copy(VideoFrame *dst, const VideoFrame *src)
{
    if ((src->codec == FMT_YV12 || src->codec == FMT_NV12) &&
        (dst->codec == FMT_YV12 || dst->codec == FMT_NV12))
    {
        d->copyctx->copy(dst, src);
        return dst->size;
    }

    AVFrame srcpic, dstpic;

    AVPictureFill(&srcpic, src);
    AVPictureFill(&dstpic, dst);

    return Copy(&dstpic, FrameTypeToPixelFormat(dst->codec),
                &srcpic, FrameTypeToPixelFormat(src->codec),
                src->width, src->height);
}

int MythAVCopy::Copy(AVFrame *pic, const VideoFrame *frame,
                 unsigned char *buffer, AVPixelFormat fmt)
{
    VideoFrameType type = PixelFormatToFrameType(fmt);
    int size = buffersize(type, frame->width, frame->height, 0) + 16;
    unsigned char *sbuf = buffer ? buffer : (unsigned char*)av_malloc(size);

    if (!sbuf)
    {
        return 0;
    }

    AVFrame pic_in;
    AVPixelFormat fmt_in = FrameTypeToPixelFormat(frame->codec);

    AVPictureFill(&pic_in, frame, fmt_in);
    av_image_fill_arrays(pic->data, pic->linesize, sbuf, fmt, frame->width, frame->height, IMAGE_ALIGN);
    return Copy(pic, fmt, &pic_in, fmt_in, frame->width, frame->height);
}

int MythAVCopy::Copy(VideoFrame *frame, const AVFrame *pic, AVPixelFormat fmt)
{
    if (fmt == AV_PIX_FMT_NV12 || AV_PIX_FMT_YUV420P)
    {
        VideoFrame framein;
        FillFrame(&framein, pic, frame->width, frame->width, frame->height, fmt);
        return Copy(frame, &framein);
    }

    AVFrame frame_out;
    AVPixelFormat fmt_out = FrameTypeToPixelFormat(frame->codec);

    AVPictureFill(&frame_out, frame, fmt_out);
    return Copy(&frame_out, fmt_out, pic, fmt, frame->width, frame->height);
}

MythPictureDeinterlacer::MythPictureDeinterlacer(AVPixelFormat pixfmt,
                                                 int width, int height, float ar)
    : m_filter_graph(nullptr)
    , m_buffersink_ctx(nullptr)
    , m_buffersrc_ctx(nullptr)
    , m_pixfmt(pixfmt)
    , m_width(width)
    , m_height(height)
    , m_ar(ar)
    , m_errored(false)
{
    if (Flush() < 0)
    {
        m_errored = true;
    }
}

int MythPictureDeinterlacer::Deinterlace(AVFrame *dst, const AVFrame *src)
{
    if (m_errored)
    {
        return -1;
    }
    if (src)
    {
        memcpy(m_filter_frame->data, src->data, sizeof(src->data));
        memcpy(m_filter_frame->linesize, src->linesize, sizeof(src->linesize));
        m_filter_frame->width = m_width;
        m_filter_frame->height = m_height;
        m_filter_frame->format = m_pixfmt;
    }
    int res = av_buffersrc_add_frame(m_buffersrc_ctx, m_filter_frame);
    if (res < 0)
    {
        return res;
    }
    res = av_buffersink_get_frame(m_buffersink_ctx, m_filter_frame);
    if (res < 0)
    {
        return res;
    }

    av_image_copy(dst->data, dst->linesize,
        (const uint8_t **)((AVFrame*)m_filter_frame)->data,
        (const int*)((AVFrame*)m_filter_frame)->linesize,
        m_pixfmt, m_width, m_height);

    av_frame_unref(m_filter_frame);

    return 0;
}

int MythPictureDeinterlacer::DeinterlaceSingle(AVFrame *dst, const AVFrame *src)
{
    if (m_errored)
    {
        return -1;
    }
    if (!m_filter_graph && Flush() < 0)
    {
        return -1;
    }
    int res = Deinterlace(dst, src);
    if (res == AVERROR(EAGAIN))
    {
        res = Deinterlace(dst, NULL);
        // We have drained the filter, we need to recreate it on the next run.
        avfilter_graph_free(&m_filter_graph);
    }
    return res;
}

int MythPictureDeinterlacer::Flush()
{
    if (m_filter_graph)
    {
        avfilter_graph_free(&m_filter_graph);
    }

    m_filter_graph = avfilter_graph_alloc();
    if (!m_filter_graph)
    {
        return -1;
    }

    AVFilterInOut *inputs = NULL, *outputs = NULL;
    AVRational ar = av_d2q(m_ar, 100000);
    QString args = QString("buffer=video_size=%1x%2:pix_fmt=%3:time_base=1/1:pixel_aspect=%4/%5[in];"
                           "[in]yadif[out];[out] buffersink")
                       .arg(m_width).arg(m_height).arg(m_pixfmt).arg(ar.num).arg(ar.den);
    int res = avfilter_graph_parse2(m_filter_graph, args.toLatin1().data(), &inputs, &outputs);
    while (1)
    {
        if (res < 0 || inputs || outputs)
        {
            break;
        }
        res = avfilter_graph_config(m_filter_graph, NULL);
        if (res < 0)
        {
            break;
        }
        if (!(m_buffersrc_ctx = avfilter_graph_get_filter(m_filter_graph, "Parsed_buffer_0")))
        {
            break;
        }
        if (!(m_buffersink_ctx = avfilter_graph_get_filter(m_filter_graph, "Parsed_buffersink_2")))
        {
            break;
        }
        return 0;
    }
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return -1;
}

MythPictureDeinterlacer::~MythPictureDeinterlacer()
{
    if (m_filter_graph)
    {
        avfilter_graph_free(&m_filter_graph);
    }
}


MythCodecMap *gCodecMap = new MythCodecMap();

MythCodecMap::MythCodecMap() : mapLock(QMutex::Recursive)
{
}

MythCodecMap::~MythCodecMap()
{
    freeAllCodecContexts();
}

AVCodecContext *MythCodecMap::getCodecContext(const AVStream *stream,
    const AVCodec *pCodec, bool nullCodec)
{
    QMutexLocker lock(&mapLock);
    AVCodecContext *avctx = streamMap.value(stream, NULL);
    if (!avctx)
    {
        if (stream == NULL || stream->codecpar == NULL)
            return NULL;
        if (nullCodec)
            pCodec = NULL;
        else
        {
            if (!pCodec)
                pCodec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (!pCodec)
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    QString("avcodec_find_decoder fail for %1").arg(stream->codecpar->codec_id));
                return NULL;
            }
        }
        avctx = avcodec_alloc_context3(pCodec);
        if (avcodec_parameters_to_context(avctx, stream->codecpar) < 0)
            avcodec_free_context(&avctx);
        if (avctx)
        {
            av_codec_set_pkt_timebase(avctx, stream->time_base);
            streamMap.insert(stream, avctx);
        }
    }
    return avctx;
}

AVCodecContext *MythCodecMap::hasCodecContext(const AVStream *stream)
{
    return streamMap.value(stream, NULL);
}

void MythCodecMap::freeCodecContext(const AVStream *stream)
{
    QMutexLocker lock(&mapLock);
    AVCodecContext *avctx = streamMap.take(stream);
    if (avctx)
        avcodec_free_context(&avctx);
}

void MythCodecMap::freeAllCodecContexts()
{
    QMutexLocker lock(&mapLock);
    QMap<const AVStream*, AVCodecContext*>::iterator i = streamMap.begin();
    while (i != streamMap.end()) {
        const AVStream *stream = i.key();
        ++i;
        freeCodecContext(stream);
    }
}
