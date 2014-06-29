//
//  mythavutil.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 7/06/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_mythavutil_h
#define MythTV_mythavutil_h

extern "C" {
#include "libavcodec/avcodec.h"
}
#include "mythframe.h"
#include "myth_imgconvert.h"

/** MythAVFrame
 * little utility class that act as a safe way to allocate an AVFrame
 * which can then be allocated on the heap. It simplifies the need to free
 * the AVFrame once done with it.
 * Example of usage:
 * {
 *   MythAVFrame frame;
 *   if (!frame)
 *   {
 *     return false
 *   }
 *
 *   frame->width = 1080;
 *
 *   AVFrame *src = frame;
 * }
 */
class MythAVFrame
{
public:
    MythAVFrame(void)
    {
        m_frame = av_frame_alloc();
    }
    ~MythAVFrame(void)
    {
        av_frame_free(&m_frame);
    }
    bool operator !() const
    {
        return m_frame == NULL;
    }
    AVFrame* operator->()
    {
        return m_frame;
    }
    AVFrame& operator*()
    {
        return *m_frame;
    }
    operator AVFrame*()
    {
        return m_frame;
    }

private:
    AVFrame *m_frame;
};

static inline AVPixelFormat FrameTypeToPixelFormat(VideoFrameType type)
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

static inline VideoFrameType PixelFormatToFrameType(AVPixelFormat fmt)
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

/**
 * AVPictureFill
 * Initialise AVPicture pic with content from VideoFrame frame
 */
static inline int AVPictureFill(AVPicture *pic, const VideoFrame *frame,
                                AVPixelFormat fmt = AV_PIX_FMT_NONE)
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

/**
 * AVPictureCopy
 * Initialise AVPicture pic, create buffer if required and copy content of
 * VideoFrame frame into it, performing the required conversion if any
 * Returns size of buffer allocated
 * Data would have to be deleted once finished with object with:
 * av_freep(pic->data[0])
 */
static inline int AVPictureCopy(AVPicture *pic, const VideoFrame *frame,
                                unsigned char *buffer = NULL,
                                AVPixelFormat fmt = AV_PIX_FMT_YUV420P)
{
    VideoFrameType type = PixelFormatToFrameType(fmt);
    int size = buffersize(type, frame->width, frame->height, 0) + 16;
    unsigned char *sbuf = buffer ? buffer : (unsigned char*)av_malloc(size);

    if (!sbuf)
    {
        return 0;
    }

    avpicture_fill(pic, sbuf, fmt, frame->width, frame->height);
    if ((type == FMT_YV12 || type == FMT_NV12) &&
        (frame->codec == FMT_NV12 || frame->codec == FMT_YV12))
    {
        copybuffer(sbuf, frame, pic->linesize[0], type);
    }
    else
    {
        AVPixelFormat fmt_in = FrameTypeToPixelFormat(frame->codec);
        AVPicture img_in;
        AVPictureFill(&img_in, frame);
        myth_sws_img_convert(pic, fmt, &img_in, fmt_in,
                             frame->width, frame->height);
    }

    return size;
}

/**
 * AVPictureCopy
 * Copy AVPicture pic into VideoFrame frame, performing the required conversion
 * Returns size of frame data
 */
static inline int AVPictureCopy(VideoFrame *frame, const AVPicture *pic,
                                AVPixelFormat fmt = AV_PIX_FMT_YUV420P)
{
    VideoFrameType type = PixelFormatToFrameType(fmt);
    int size = buffersize(type, frame->width, frame->height, 0) + 16;
    unsigned char *sbuf = (unsigned char*)av_malloc(size);

    if ((type == FMT_YV12 || type == FMT_NV12) &&
        (frame->codec == FMT_NV12 || frame->codec == FMT_YV12))
    {
        copybuffer(sbuf, frame, pic->linesize[0], type);
    }
    else
    {
        // Can't handle those natively, convert it first
        AVPixelFormat fmt_out = FrameTypeToPixelFormat(frame->codec);
        AVPicture img_out;
        AVPictureFill(&img_out, frame);
        myth_sws_img_convert(&img_out, fmt_out, pic, fmt,
                             frame->width, frame->height);
    }
    return frame->size;
}

#endif
