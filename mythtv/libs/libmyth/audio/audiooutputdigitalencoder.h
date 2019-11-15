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

    bool   Init(AVCodecID codec_id, int bitrate, int samplerate, int channels);
    size_t Encode(void *buf, int len, AudioFormat format);
    size_t GetFrames(void *ptr, int maxlen);
    int    Buffered(void) const
    { return m_inlen / sizeof(inbuf_t) / m_av_context->channels; }
    void    clear();

  private:
    void   Reset(void);
    static void *realloc(void *ptr, size_t old_size, size_t new_size);

    AVCodecContext *m_av_context        {nullptr};
    outbuf_t       *m_out               {nullptr};
    size_t          m_out_size          {0};
    inbuf_t        *m_in                {nullptr};
    inbuf_t        *m_inp               {nullptr};
    size_t          m_in_size           {0};
    int             m_outlen            {0};
    int             m_inlen             {0};
    size_t          m_samples_per_frame {0};
    SPDIFEncoder   *m_spdifenc          {nullptr};
    AVFrame        *m_frame             {nullptr};
};

#endif
