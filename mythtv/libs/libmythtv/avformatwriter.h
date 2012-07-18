#ifndef AVFORMATWRITER_H_
#define AVFORMATWRITER_H_

#include "filewriterbase.h"
#include "avfringbuffer.h"

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

    bool WriteVideoFrame(VideoFrame *frame);
    bool WriteAudioFrame(unsigned char *buf, int fnum, int timecode);
    bool WriteTextFrame(int vbimode, unsigned char *buf, int len,
                        int timecode, int pagenr);

    bool NextFrameIsKeyFrame(void);
    bool ReOpen(QString filename);

  private:
    AVStream *AddVideoStream(void);
    bool OpenVideo(void);
    AVStream *AddAudioStream(void);
    bool OpenAudio(void);
    AVFrame *AllocPicture(enum PixelFormat pix_fmt);

    AVRational GetCodecTimeBase(void);

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
    AVPacket              *m_pkt;
    AVFrame               *m_audPicture;
    AVPacket              *m_audPkt;
    unsigned char         *m_videoOutBuf;
    unsigned char         *m_audioOutBuf;
    int                    m_audioOutBufSize;
    float                 *m_audioFltBuf;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

