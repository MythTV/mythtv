#ifndef AVFORMATWRITER_H_
#define AVFORMATWRITER_H_

// Qt
#include <QList>

// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythtv/io/mythavformatbuffer.h"
#include "libmythtv/io/mythmediawriter.h"
#include "libmythtv/mythavutil.h"

#undef HAVE_AV_CONFIG_H
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class MTV_PUBLIC MythAVFormatWriter : public MythMediaWriter
{
  public:
    MythAVFormatWriter() = default;
   ~MythAVFormatWriter() override;

    bool Init                (void) override;
    bool OpenFile            (void) override;
    bool CloseFile           (void) override;
    int  WriteVideoFrame     (MythVideoFrame *Frame) override;
    int  WriteAudioFrame     (unsigned char *Buffer, int FrameNumber, std::chrono::milliseconds &Timecode) override;
    int  WriteTextFrame      (int VBIMode, unsigned char *Buffer, int Length,
                              std::chrono::milliseconds Timecode, int PageNumber) override;
    int  WriteSeekTable      (void) override;
    bool SwitchToNextFile    (void) override;

    bool NextFrameIsKeyFrame (void);
    bool ReOpen              (const QString& Filename);

  private:
    bool      openFileHelper();
    AVStream* AddVideoStream (void);
    bool      OpenVideo      (void);
    AVStream* AddAudioStream (void);
    bool      OpenAudio      (void);
    AVFrame*  AllocPicture   (enum AVPixelFormat PixFmt);
    void      Cleanup        (void);
    AVRational  GetCodecTimeBase (void);
    static bool FindAudioFormat  (AVCodecContext *Ctx, const AVCodec *Codec, AVSampleFormat Format);

    MythAVFormatBuffer    *m_avfBuffer     { nullptr };
    MythMediaBuffer       *m_buffer        { nullptr };
    AVOutputFormat         m_fmt           { };
    AVFormatContext       *m_ctx           { nullptr };
    MythCodecMap           m_codecMap;
    AVStream              *m_videoStream   { nullptr };
    const AVCodec         *m_avVideoCodec  { nullptr };
    AVStream              *m_audioStream   { nullptr };
    const AVCodec         *m_avAudioCodec  { nullptr };
    AVFrame               *m_picture       { nullptr };
    AVFrame               *m_audPicture    { nullptr };
    unsigned char         *m_audioInBuf    { nullptr };
    unsigned char         *m_audioInPBuf   { nullptr };
    QList<std::chrono::milliseconds> m_bufferedVideoFrameTimes;
    QList<int>             m_bufferedVideoFrameTypes;
    QList<std::chrono::milliseconds> m_bufferedAudioFrameTimes;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

