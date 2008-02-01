#ifndef AUDIOOUTPUTREENCODER
#define AUDIOOUTPUTREENCODER

extern "C" {
#include "libavcodec/avcodec.h"
};

class AudioOutputDigitalEncoder
{
  public:
    AudioOutputDigitalEncoder(void);
    ~AudioOutputDigitalEncoder();

    bool   Init(CodecID codec_id, int bitrate, int samplerate, int channels);
    void   Dispose(void);
    size_t Encode(short * buff);

    inline char *GetFrameBuffer(void);
    size_t FrameSize(void)  const { return one_frame_bytes; }
    char  *GetOutBuff(void) const { return outbuf;          }

  public:
    size_t audio_bytes_per_sample;

  private:
    AVCodecContext *av_context;
    char           *outbuf;
    int             outbuf_size;
    char           *frame_buffer;
    size_t          one_frame_bytes;
};

inline char *AudioOutputDigitalEncoder::GetFrameBuffer(void)
{
    if (!frame_buffer && av_context)
        frame_buffer = new char [one_frame_bytes];

    return frame_buffer; 
}

#endif
