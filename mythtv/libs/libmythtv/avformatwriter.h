#ifndef AVFORMATWRITER_H_
#define AVFORMATWRITER_H_

#include "mythconfig.h"
#include "filewriterbase.h"
#include "avfringbuffer.h"

#include <QList>

#undef HAVE_AV_CONFIG_H
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class MTV_PUBLIC AVFormatWriter : public FileWriterBase
{
  public:
    AVFormatWriter() {};
   ~AVFormatWriter();

    bool Init(void) override; // FileWriterBase
    bool OpenFile(void) override; // FileWriterBase
    bool CloseFile(void) override; // FileWriterBase

    int  WriteVideoFrame(VideoFrame *frame) override; // FileWriterBase
    int  WriteAudioFrame(unsigned char *buf, int fnum, long long &timecode) override; // FileWriterBase
    int  WriteTextFrame(int vbimode, unsigned char *buf, int len,
                        long long timecode, int pagenr) override; // FileWriterBase

    bool NextFrameIsKeyFrame(void);
    bool ReOpen(const QString& filename);

  private:
    AVStream *AddVideoStream(void);
    bool OpenVideo(void);
    AVStream *AddAudioStream(void);
    bool OpenAudio(void);
    AVFrame *AllocPicture(enum AVPixelFormat pix_fmt);
    void Cleanup(void);

    AVRational GetCodecTimeBase(void);
    static bool FindAudioFormat(AVCodecContext *ctx, AVCodec *c, AVSampleFormat format);

    AVFRingBuffer         *m_avfRingBuffer {nullptr};
    RingBuffer            *m_ringBuffer    {nullptr};

    AVOutputFormat         m_fmt           {};
    AVFormatContext       *m_ctx           {nullptr};
    AVStream              *m_videoStream   {nullptr};
    AVCodec               *m_avVideoCodec  {nullptr};
    AVStream              *m_audioStream   {nullptr};
    AVCodec               *m_avAudioCodec  {nullptr};
    AVFrame               *m_picture       {nullptr};
    AVFrame               *m_audPicture    {nullptr};
    unsigned char         *m_audioInBuf    {nullptr};
    unsigned char         *m_audioInPBuf   {nullptr};

    QList<long long>       m_bufferedVideoFrameTimes;
    QList<int>             m_bufferedVideoFrameTypes;
    QList<long long>       m_bufferedAudioFrameTimes;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

