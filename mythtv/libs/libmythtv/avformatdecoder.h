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

    int OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048]);
    void GetFrame(int onlyvideo);

    bool DoRewind(long long desiredFrame);
    bool DoFastForward(long long desiredFrame);

    char *GetScreenGrab(int secondsin);

    bool isLastFrameKey(void) { return false; }
    void WriteStoredData(RingBuffer *rb) { }
    void SetRawFrameState(bool state) { (void)state; }
    bool GetRawFrameState(void) { return false; }

  protected:
    RingBuffer *getRingBuf(void) { return ringBuffer; }

  private:
    friend int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);

    friend int open_avf(URLContext *h, const char *filename, int flags);
    friend int read_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend int write_avf(URLContext *h, uint8_t *buf, int buf_size);
    friend offset_t seek_avf(URLContext *h, offset_t offset, int whence);
    friend int close_avf(URLContext *h);

    void InitByteContext(void);

    RingBuffer *ringBuffer;

    AVFormatContext *ic;
    AVFormatParameters params;
    AVPacket *pkt;

    URLContext readcontext;

    unsigned char *directbuf;

    int frame_decoded;

    bool directrendering;
    bool drawband;
    long long framesPlayed;

    int audio_sample_size;
    int audio_sampling_rate;

    long long lastapts;
    long long lastvpts;

    bool hasbframes;
};

#endif
