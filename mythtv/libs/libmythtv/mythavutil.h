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

extern "C" {
#include "libavcodec/avcodec.h"
}

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

typedef struct VideoFrame_ VideoFrame;

/**
 * AVPictureFill
 * Initialise AVPicture pic with content from VideoFrame frame
 */
int MTV_PUBLIC AVPictureFill(AVPicture *pic, const VideoFrame *frame,
                             AVPixelFormat fmt = AV_PIX_FMT_NONE);

/**
 * AVPictureCopy
 * Initialise AVPicture pic, create buffer if required and copy content of
 * VideoFrame frame into it, performing the required conversion if any
 * Returns size of buffer allocated
 * Data would have to be deleted once finished with object with:
 * av_freep(pic->data[0])
 */
int MTV_PUBLIC AVPictureCopy(AVPicture *pic, const VideoFrame *frame,
                             unsigned char *buffer = NULL,
                             AVPixelFormat fmt = AV_PIX_FMT_YUV420P);

/**
 * AVPictureCopy
 * Copy AVPicture pic into VideoFrame frame, performing the required conversion
 * Returns size of frame data
 */
int MTV_PUBLIC AVPictureCopy(VideoFrame *frame, const AVPicture *pic,
                             AVPixelFormat fmt = AV_PIX_FMT_YUV420P);

#endif
