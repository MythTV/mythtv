#ifndef AUDIOOUTPUTREENCODER
#define AUDIOOUTPUTREENCODER

extern "C" {
#include "libavcodec/avcodec.h"
};

#define INBUFSIZE 131072
#define OUTBUFSIZE 98304

class AudioOutputDigitalEncoder
{
  public:
    AudioOutputDigitalEncoder(void);
    ~AudioOutputDigitalEncoder();

    bool   Init(CodecID codec_id, int bitrate, int samplerate, int channels);
    void   Dispose(void);
    size_t Encode(void *buf, int len);
    void   GetFrames(void *ptr, int maxlen);
    size_t FrameSize(void)  const { return one_frame_bytes; }
    int    Buffered(void) const { return inbuflen; }

  public:
    size_t audio_bytes_per_sample;

  private:
    AVCodecContext *av_context;
    char            outbuf[OUTBUFSIZE];
    char            inbuf[INBUFSIZE];
    int             outbuflen;
    int             inbuflen;
    size_t          one_frame_bytes;
};

#endif
