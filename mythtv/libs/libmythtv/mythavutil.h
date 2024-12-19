#ifndef MYTHAVUTIL_H
#define MYTHAVUTIL_H

#include <utility>

// Qt
#include <QMap>
#include <QRecursiveMutex>
#include <QVector>

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"

#include "libavutil/buffer.h"
}

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythtv/mythframe.h"
#include "libmythui/mythhdr.h"

struct SwsContext;
struct AVStream;
struct AVCodecContext;

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
    QRecursiveMutex m_mapLock;
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
    static void DeinterlaceAVFrame(AVFrame* Frame);
    static int  FillAVFrame(AVFrame* Frame, const MythVideoFrame* From, AVPixelFormat Fmt = AV_PIX_FMT_NONE);
    static AVPixelFormat  FrameTypeToPixelFormat(VideoFrameType Type);
    static VideoFrameType PixelFormatToFrameType(AVPixelFormat Fmt);
    static MythHDR::HDRType FFmpegTransferToHDRType(int Transfer);
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

/**
@brief C++ wrapper for AVBufferRef.

Using this avoids littering the code with manual memory management.

You must verify that the instance has_buffer() before calling any other function.
*/
class MTV_PUBLIC MythAVBufferRef {
  public:
    /**
    @param buf The AVBufferRef* to reference, may be NULL.
    */
    explicit MythAVBufferRef(const AVBufferRef* buf = nullptr) : m_buffer(ref(buf)) {}
    ~MythAVBufferRef() { unref(); }

    // Copy constructor
    MythAVBufferRef(const MythAVBufferRef& other) : MythAVBufferRef(other.m_buffer) {}
    // Move constructor
    MythAVBufferRef(MythAVBufferRef&& other) noexcept : MythAVBufferRef() { swap(*this, other); }
    // Copy assignment operator
    MythAVBufferRef& operator=(MythAVBufferRef other)
    {
        swap(*this, other);
        return *this;
    }
    // Move assignment operator
    MythAVBufferRef& operator=(MythAVBufferRef&& other) noexcept
    {
        // release resources held by this, but prevent suicide on self-assignment
        MythAVBufferRef tmp {std::move(other)};
        swap(*this, tmp);
        return *this;
    }

    friend void swap(MythAVBufferRef& a, MythAVBufferRef& b) noexcept
    {
        using std::swap;
        swap(a.m_buffer, b.m_buffer);
    }

    bool has_buffer()       { return m_buffer != nullptr; }

    const uint8_t*  data()  { return m_buffer->data; }
    size_t          size()  { return m_buffer->size; }
  private:
    static AVBufferRef* ref(const AVBufferRef* buf)
    {
        if (buf == nullptr)
        {
            return nullptr;
        }
        AVBufferRef* reference = av_buffer_ref(buf);
        if (reference == nullptr)
        {
            LOG(VB_GENERAL, LOG_ERR, "av_buffer_ref() failed to allocate memory.");
        }
        return reference;
    }
    void unref() { av_buffer_unref(&m_buffer); }

    AVBufferRef* m_buffer {nullptr};
};


#endif
