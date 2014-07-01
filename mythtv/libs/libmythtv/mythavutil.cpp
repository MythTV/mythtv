//
//  mythavutil.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 28/06/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//

#include "mythframe.h"
#include "mythavutil.h"
extern "C" {
#include "libswscale/swscale.h"
}

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

int AVPictureFill(AVPicture *pic, const VideoFrame *frame, AVPixelFormat fmt)
{
    if (fmt == AV_PIX_FMT_NONE)
    {
        fmt = FrameTypeToPixelFormat(frame->codec);
    }

    avpicture_fill(pic, frame->buf, fmt, frame->width, frame->height);
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
    MythAVCopyPrivate(bool uswc)
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
        size    = avpicture_get_size(_fmt, _width, _height);
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

void MythAVCopy::FillFrame(VideoFrame *frame, const AVPicture *pic, int pitch,
                           int width, int height, AVPixelFormat pix_fmt)
{
    int size = avpicture_get_size(pix_fmt, width, height);

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

int MythAVCopy::Copy(AVPicture *dst, AVPixelFormat dst_pix_fmt,
                 const AVPicture *src, AVPixelFormat pix_fmt,
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

    d->swsctx = sws_getCachedContext(d->swsctx, width, height, pix_fmt,
                                     width, height, dst_pix_fmt,
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

    AVPicture srcpic, dstpic;

    AVPictureFill(&srcpic, src);
    AVPictureFill(&dstpic, dst);

    return Copy(&dstpic, FrameTypeToPixelFormat(dst->codec),
                &srcpic, FrameTypeToPixelFormat(src->codec),
                src->width, src->height);
}

int MythAVCopy::Copy(AVPicture *pic, const VideoFrame *frame,
                 unsigned char *buffer, AVPixelFormat fmt)
{
    VideoFrameType type = PixelFormatToFrameType(fmt);
    int size = buffersize(type, frame->width, frame->height, 0) + 16;
    unsigned char *sbuf = buffer ? buffer : (unsigned char*)av_malloc(size);

    if (!sbuf)
    {
        return 0;
    }

    AVPicture pic_in;
    AVPixelFormat fmt_in = FrameTypeToPixelFormat(frame->codec);

    AVPictureFill(&pic_in, frame, fmt_in);
    avpicture_fill(pic, sbuf, fmt, frame->width, frame->height);
    return Copy(pic, fmt, &pic_in, fmt_in, frame->width, frame->height);
}

int MythAVCopy::Copy(VideoFrame *frame, const AVPicture *pic, AVPixelFormat fmt)
{
    if (fmt == AV_PIX_FMT_NV12 || AV_PIX_FMT_YUV420P)
    {
        VideoFrame framein;
        FillFrame(&framein, pic, frame->width, frame->width, frame->height, fmt);
        return Copy(frame, &framein);
    }

    AVPicture frame_out;
    AVPixelFormat fmt_out = FrameTypeToPixelFormat(frame->codec);

    AVPictureFill(&frame_out, frame, fmt_out);
    return Copy(&frame_out, fmt_out, pic, fmt, frame->width, frame->height);
}
