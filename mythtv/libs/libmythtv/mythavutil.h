//
//  mythavutil.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 7/06/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_mythavutil_h
#define MythTV_mythavutil_h

#include "mythframe.h"
extern "C" {
#include "libavcodec/avcodec.h"
}

#include <QMap>
#include <QMutex>
#include <QVector>

struct AVFilterGraph;
struct AVFilterContext;
struct AVStream;
struct AVCodecContext;

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
        return m_frame == nullptr;
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

private:
    AVFrame *m_frame {nullptr};
};

class MTV_PUBLIC MythCodecMap
{
  public:
    MythCodecMap() = default;
   ~MythCodecMap();
    AVCodecContext* GetCodecContext(const AVStream* Stream,
                                    const AVCodec* Codec = nullptr,
                                    bool NullCodec = false);
    AVCodecContext* FindCodecContext(const AVStream* Stream);
    void FreeCodecContext(const AVStream* Stream);
    void FreeAllContexts();

  private:
    QMap<const AVStream*, AVCodecContext*> m_streamMap;
    QMutex m_mapLock {QMutex::Recursive};
};

class MythAVCopyPrivate;

/**
 * MythAVCopy
 * Copy AVFrame<->frame, performing the required conversion if any
 */
class MTV_PUBLIC MythAVCopy
{
  public:
    MythAVCopy();
    virtual ~MythAVCopy();

    int Copy(MythVideoFrame *dst, const MythVideoFrame *src);
    /**
     * Copy
     * Initialise AVFrame pic, create buffer if required and copy content of
     * VideoFrame frame into it, performing the required conversion if any
     * Returns size of buffer allocated
     * Data would have to be deleted once finished with object with:
     * av_freep(pic->data[0])
     */
    int Copy(AVFrame *pic, const MythVideoFrame *frame,
             unsigned char *buffer = nullptr,
             AVPixelFormat fmt = AV_PIX_FMT_YUV420P);
    int Copy(AVFrame *dst, AVPixelFormat dst_pix_fmt,
             const AVFrame *src, AVPixelFormat pix_fmt,
             int width, int height);

  private:
    Q_DISABLE_COPY(MythAVCopy)
    MythAVCopyPrivate *d {nullptr}; // NOLINT(readability-identifier-naming)
};

/**
 * AVPictureFill
 * Initialise AVFrame pic with content from VideoFrame frame
 */
int MTV_PUBLIC AVPictureFill(AVFrame *pic, const MythVideoFrame *frame,
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
    MythPictureDeinterlacer(AVPixelFormat pixfmt, int width, int height, float ar = 1.0F);
    ~MythPictureDeinterlacer();
    // Will deinterlace src into dst. If EAGAIN is returned, more frames
    // are needed to output a frame.
    // To drain the deinterlacer, call Deinterlace with src = nullptr until you
    // get no more frames. Once drained, you must call Flush() to start
    // deinterlacing again.
    int Deinterlace(AVFrame *dst, const AVFrame *src);
    int DeinterlaceSingle(AVFrame *dst, const AVFrame *src);
    // Flush and reset the deinterlacer.
    int Flush();
private:
    AVFilterGraph*      m_filterGraph   {nullptr};
    MythAVFrame         m_filterFrame;
    AVFilterContext*    m_bufferSinkCtx {nullptr};
    AVFilterContext*    m_bufferSrcCtx  {nullptr};
    AVPixelFormat       m_pixfmt        {AV_PIX_FMT_NONE};
    int                 m_width         {0};
    int                 m_height        {0};
    float               m_ar;
    bool                m_errored       {false};
};


class MTV_PUBLIC MythStreamInfo {
public:
    // These are for All types
    char                m_codecType {' '};   // V=video, A=audio, S=subtitle
    QString             m_codecName;
    int64_t             m_duration {0};
    // These are for Video only
    int                 m_width {0};
    int                 m_height {0};
    float               m_SampleAspectRatio {0.0};
    // AV_FIELD_TT,          //< Top coded_first, top displayed first
    // AV_FIELD_BB,          //< Bottom coded first, bottom displayed first
    // AV_FIELD_TB,          //< Top coded first, bottom displayed first
    // AV_FIELD_BT,          //< Bottom coded first, top displayed first
    QString             m_fieldOrder {"UN"};   // UNknown, PRogressive, TT, BB, TB, BT
    float               m_frameRate {0.0};
    float               m_avgFrameRate {0.0};
    // This is for audio only
    int                 m_channels {0};
};


/*
*   Class to get stream info, used by service Video/GetStreamInfo
*/
class MTV_PUBLIC MythStreamInfoList {
public:
    explicit MythStreamInfoList(const QString& filename);
    int                     m_errorCode         {0};
    QString                 m_errorMsg;
    QVector<MythStreamInfo> m_streamInfoList;
};

#endif
