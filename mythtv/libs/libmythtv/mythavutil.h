//
//  mythavutil.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 7/06/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_mythavutil_h
#define MythTV_mythavutil_h

// Qt
#include <QMap>
#include <QMutex>
#include <QVector>

// MythTV
#include "mythframe.h"

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"
}

struct SwsContext;
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

class MTV_PUBLIC MythAVCopy
{
  public:
    MythAVCopy() = default;
   ~MythAVCopy();
    int Copy(AVFrame* To, const MythVideoFrame* From, unsigned char* Buffer,
             AVPixelFormat Fmt = AV_PIX_FMT_YUV420P);
    int Copy(AVFrame* To, AVPixelFormat ToFmt, const AVFrame* From, AVPixelFormat FromFmt,
             int Width, int Height);

  private:
    Q_DISABLE_COPY(MythAVCopy)
    int SizeData(int Width, int Height, AVPixelFormat Fmt);

    AVPixelFormat m_format  { AV_PIX_FMT_NONE };
    SwsContext*   m_swsctx  { nullptr };
    int           m_width   { 0 };
    int           m_height  { 0 };
    int           m_size    { 0 };
};

class MTV_PUBLIC MythAVUtil
{
  public:
    static int FillAVFrame(AVFrame* Frame, const MythVideoFrame* From,
                           AVPixelFormat Fmt = AV_PIX_FMT_NONE);
    static AVPixelFormat  FrameTypeToPixelFormat(VideoFrameType Type);
    static VideoFrameType PixelFormatToFrameType(AVPixelFormat Fmt);
};

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
