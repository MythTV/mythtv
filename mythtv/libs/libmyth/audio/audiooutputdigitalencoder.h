#ifndef AUDIOOUTPUTREENCODER
#define AUDIOOUTPUTREENCODER

extern "C" {
#include "libavcodec/avcodec.h"
};

#include "spdifencoder.h"
#include "audiooutputsettings.h"

#define INBUFSIZE 131072
#define OUTBUFSIZE INBUFSIZE

class AudioOutputDigitalEncoder
{
    typedef int16_t inbuf_t;
    typedef int16_t outbuf_t;

  public:
    AudioOutputDigitalEncoder(void);
    ~AudioOutputDigitalEncoder();

    bool   Init(CodecID codec_id, int bitrate, int samplerate, int channels);
    size_t Encode(void *buf, int len, AudioFormat format);
    size_t GetFrames(void *ptr, int maxlen);
    int    Buffered(void) const
    { return inlen / sizeof(inbuf_t) / av_context->channels; }
    void    clear();

  private:
    void   Reset(void);
    void  *realloc(void *ptr, size_t old_size, size_t new_size);

    AVCodecContext *av_context;
    outbuf_t       *out;
    size_t          out_size;
    inbuf_t        *in, *inp;
    size_t          in_size;
    int             outlen;
    int             inlen;
    size_t          samples_per_frame;
    SPDIFEncoder   *m_spdifenc;
};

#endif
