#ifndef SPDIFENCODER_H_
#define SPDIFENCODER_H_

#include <QString>

#include "output.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavcodec/audioconvert.h"
}

#pragma weak av_guess_format
#pragma weak avformat_alloc_context
#pragma weak av_new_stream
#pragma weak av_write_header
#pragma weak av_write_frame
#pragma weak av_write_trailer
#pragma weak avcodec_close
#pragma weak av_freep
#pragma weak av_set_parameters
#pragma weak av_alloc_put_byte

class MPUBLIC SPDIFEncoder
{
  public:
    SPDIFEncoder(QString muxer, AVCodecContext *ctx);
    ~SPDIFEncoder();
    void WriteFrame(unsigned char *data, int size);
    int  GetData(unsigned char *buffer, int &dest_size);
    void Reset();
    bool Succeeded()  { return m_complete; };
    bool SetMaxMuxRate(int rate);
    int  GetMaxMuxRate();
    int  GetBitrate();

  private:
    static int funcIO(void *opaque, unsigned char *buf, int size);
    void Destroy();

  private:
    bool                m_complete;
    AVFormatContext    *m_oc;
    AVStream           *m_stream;
    unsigned char       m_buffer[AVCODEC_MAX_AUDIO_FRAME_SIZE];
    long                m_size;
};

#endif
