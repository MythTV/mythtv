#ifndef AVFORMATDECODER_H_
#define AVFORMATDECODER_H_

#include <qstring.h>

#include "decoderbase.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
}

class AvFormatDecoder : public DecoderBase
{
  public:
    AvFormatDecoder(NuppelVideoPlayer *parent);
   ~AvFormatDecoder();

    void Reset(void);

    static bool CanHandle(char testbuf[2048], const QString &filename);

    int OpenFile(RingBuffer *rbuffer, bool novideo);
    void GetFrame(int onlyvideo);

    bool DoRewind(long long desiredFrame);
    bool DoFastForward(long long desiredFrame);

    char *GetScreenGrab(int secondsin);

  private:
    friend int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);

    AVFormatContext *ic;
    AVFormatParameters params;
    AVPacket *pkt;

    unsigned char *directbuf;

    int frame_decoded;

    bool directrendering;
    long long framesPlayed;

    int audio_sample_size;
    int audio_sampling_rate;

    long long lastapts;
    long long lastvpts;
};

#endif
