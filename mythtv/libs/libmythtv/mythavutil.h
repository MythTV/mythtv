//
//  mythavutil.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 7/06/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_mythavutil_h
#define MythTV_mythavutil_h

#include "mythtvexp.h" // for MUNUSED
#include "mythframe.h"
extern "C" {
#include "libavcodec/avcodec.h"
}

struct AVFilterGraph;
struct AVFilterContext;

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
    AVFrame* operator->() const
    {
        return m_frame;
    }
    AVFrame& operator*() const
    {
        return *m_frame;
    }
    operator AVFrame*() const
    {
        return m_frame;
    }
    operator const AVFrame*() const
    {
        return m_frame;
    }
    operator AVPicture*() const
    {
        return reinterpret_cast<AVPicture*>(m_frame);
    }
    operator const AVPicture*() const
    {
        return reinterpret_cast<AVPicture*>(m_frame);
    }

private:
    AVFrame *m_frame;
};

class MythAVCopyPrivate;

/**
 * MythAVCopy
 * Copy AVPicture<->frame, performing the required conversion if any
 */
class MTV_PUBLIC MythAVCopy
{
public:
    MythAVCopy(bool USWC=true);
    virtual ~MythAVCopy();

    int Copy(VideoFrame *dst, const VideoFrame *src);
    /**
     * Copy
     * Initialise AVPicture pic, create buffer if required and copy content of
     * VideoFrame frame into it, performing the required conversion if any
     * Returns size of buffer allocated
     * Data would have to be deleted once finished with object with:
     * av_freep(pic->data[0])
     */
    int Copy(AVPicture *pic, const VideoFrame *frame,
             unsigned char *buffer = NULL,
             AVPixelFormat fmt = AV_PIX_FMT_YUV420P);
    /**
     * Copy
     * Copy AVPicture pic into VideoFrame frame, performing the required conversion
     * Returns size of frame data
     */
    int Copy(VideoFrame *frame, const AVPicture *pic,
             AVPixelFormat fmt = AV_PIX_FMT_YUV420P);
    int Copy(AVPicture *dst, AVPixelFormat dst_pix_fmt,
             const AVPicture *src, AVPixelFormat pix_fmt,
             int width, int height);

private:
    void FillFrame(VideoFrame *frame, const AVPicture *pic, int pitch,
                   int width, int height, AVPixelFormat pix_fmt);
    MythAVCopyPrivate *d;
};

/**
 * AVPictureFill
 * Initialise AVPicture pic with content from VideoFrame frame
 */
int MTV_PUBLIC AVPictureFill(AVPicture *pic, const VideoFrame *frame,
                             AVPixelFormat fmt = AV_PIX_FMT_NONE);

/**
 * Convert VideoFrameType into FFmpeg's PixelFormat equivalent and
 * vice-versa.
 */
MTV_PUBLIC AVPixelFormat FrameTypeToPixelFormat(VideoFrameType type);
MTV_PUBLIC VideoFrameType PixelFormatToFrameType(AVPixelFormat fmt);

/**
 * MythPictureDeinterlacer
 * simple deinterlacer based on FFmpeg's yadif filter.
 * Yadif requires 3 frames before starting to return a deinterlaced frame.
 */
class MTV_PUBLIC MythPictureDeinterlacer
{
public:
    MythPictureDeinterlacer(AVPixelFormat pixfmt, int width, int height, float ar = 1.0f);
    ~MythPictureDeinterlacer();
    // Will deinterlace src into dst. If EAGAIN is returned, more frames
    // are needed to output a frame.
    // To drain the deinterlacer, call Deinterlace with src = NULL until you
    // get no more frames. Once drained, you must call Flush() to start
    // deinterlacing again.
    int Deinterlace(AVPicture *dst, const AVPicture *src);
    int DeinterlaceSingle(AVPicture *dst, const AVPicture *src);
    // Flush and reset the deinterlacer.
    int Flush();
private:
    AVFilterGraph*      m_filter_graph;
    MythAVFrame         m_filter_frame;
    AVFilterContext*    m_buffersink_ctx;
    AVFilterContext*    m_buffersrc_ctx;
    AVPixelFormat       m_pixfmt;
    int                 m_width;
    int                 m_height;
    float               m_ar;
    bool                m_errored;
};
#endif
