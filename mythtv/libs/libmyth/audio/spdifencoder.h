#ifndef SPDIFENCODER_H_
#define SPDIFENCODER_H_

#include <QString>

#include "output.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavcodec/audioconvert.h"
}

class MPUBLIC SPDIFEncoder
{

  public:
    SPDIFEncoder(QString muxer, AVCodecContext *ctx);
    ~SPDIFEncoder();
    void WriteFrame(unsigned char *data, int size);
    int  GetData(unsigned char *buffer, int &dest_size);
    void Reset();
    
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
