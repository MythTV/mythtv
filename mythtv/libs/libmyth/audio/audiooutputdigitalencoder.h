#ifndef AUDIOOUTPUTREENCODER
#define AUDIOOUTPUTREENCODER

extern "C" {
#include "libavcodec/avcodec.h"
};

#include "spdifencoder.h"

#define INBUFSIZE 131072
#define OUTBUFSIZE 98304
#define ENCODER_INBUFSIZE INBUFSIZE

class AudioOutputDigitalEncoder
{
  public:
    AudioOutputDigitalEncoder(void);
    ~AudioOutputDigitalEncoder();

    bool   Init(CodecID codec_id, int bitrate, int samplerate, int channels);
    void   Dispose(void);
    size_t Encode(void *buf, int len, bool isFloat);
    void   GetFrames(void *ptr, int maxlen);
    size_t FrameSize(void)  const { return one_frame_bytes; }
    int    Buffered(void) const { return inlen; }

  public:
    size_t bytes_per_sample;

  private:
    AVCodecContext *av_context;
    unsigned char   out[OUTBUFSIZE];
    unsigned char   inbuf[INBUFSIZE+16];
    unsigned char  *in;
    int             outlen;
    int             inlen;
    size_t          one_frame_bytes;
    uint8_t         m_encodebuffer[FF_MIN_BUFFER_SIZE];
    SPDIFEncoder   *m_spdifenc;
};

#endif
