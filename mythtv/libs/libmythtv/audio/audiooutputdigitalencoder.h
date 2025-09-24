#ifndef AUDIOOUTPUTREENCODER
#define AUDIOOUTPUTREENCODER

extern "C" {
#include "libavcodec/avcodec.h"
};

#include "spdifencoder.h"
#include "audiooutputsettings.h"

static constexpr ssize_t INBUFSIZE  {    131072 };
static constexpr ssize_t OUTBUFSIZE { INBUFSIZE };

class AudioOutputDigitalEncoder
{

  public:
    AudioOutputDigitalEncoder(void);
    ~AudioOutputDigitalEncoder();

    bool   Init(AVCodecID codec_id, int bitrate, int samplerate, int channels);
    int Encode(void *input, int len, AudioFormat format);
    int GetFrames(void *ptr, int maxlen);
    int    Buffered(void) const
    // assume 32 bit samples = 4 byte
    { return m_inlen / 4 / m_avContext->ch_layout.nb_channels; }
    void    clear();

  private:
    void   Reset(void);
    static void *realloc(void *ptr, size_t old_size, size_t new_size);

    AVCodecContext *m_avContext         {nullptr};
    uint8_t        *m_outbuf            {nullptr};
    ssize_t         m_outSize           {0};
    // m_inbuf  = 6 channel data converted to S32 or FLT samples interleaved
    uint8_t        *m_inbuf             {nullptr};
    // m_framebuf = 1 frame, deinterleaved into planar format
    uint8_t        *m_framebuf          {nullptr};
    ssize_t         m_inSize            {0};
    int             m_outlen            {0};
    // m_inlen = number of bytes available in m_inbuf
    int             m_inlen             {0};
    int             m_samplesPerFrame   {0};
    SPDIFEncoder   *m_spdifEnc          {nullptr};
    AVFrame        *m_frame             {nullptr};
};

#endif
