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
    AVFormatWriter();
   ~AVFormatWriter();

    bool Init(void);
    bool OpenFile(void);
    bool CloseFile(void);

    int  WriteVideoFrame(VideoFrame *frame);
    int  WriteAudioFrame(unsigned char *buf, int fnum, long long &timecode);
    int  WriteTextFrame(int vbimode, unsigned char *buf, int len,
                        long long timecode, int pagenr);

    bool NextFrameIsKeyFrame(void);
    bool ReOpen(QString filename);

  private:
    AVStream *AddVideoStream(void);
    bool OpenVideo(void);
    AVStream *AddAudioStream(void);
    bool OpenAudio(void);
    AVFrame *AllocPicture(enum PixelFormat pix_fmt);

    AVRational GetCodecTimeBase(void);
    bool FindAudioFormat(AVCodecContext *ctx, AVCodec *c, AVSampleFormat format);

    AVFRingBuffer         *m_avfRingBuffer;
    RingBuffer            *m_ringBuffer;

    AVOutputFormat         m_fmt;
    AVFormatContext       *m_ctx;
    AVStream              *m_videoStream;
    AVCodec               *m_avVideoCodec;
    AVStream              *m_audioStream;
    AVCodec               *m_avAudioCodec;
    AVFrame               *m_picture;
    AVFrame               *m_tmpPicture;
    AVFrame               *m_audPicture;
    unsigned char         *m_audioInBuf;
    unsigned char         *m_audioInPBuf;

    QList<long long>       m_bufferedVideoFrameTimes;
    QList<int>             m_bufferedVideoFrameTypes;
    QList<long long>       m_bufferedAudioFrameTimes;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

