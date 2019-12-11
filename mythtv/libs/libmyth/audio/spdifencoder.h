#ifndef SPDIFENCODER_H_
#define SPDIFENCODER_H_

#include <QString>

#include "output.h"
#include "audiooutput.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class MPUBLIC SPDIFEncoder
{
  public:
    SPDIFEncoder(const QString& muxer, AVCodecID codec_id);
    ~SPDIFEncoder();
    void WriteFrame(unsigned char *data, int size);
    int  GetData(unsigned char *buffer, int &dest_size);
    int  GetProcessedSize() { return m_size; };
    unsigned char *GetProcessedBuffer() { return m_buffer; };
    void Reset();
    bool Succeeded()  { return m_complete; };
    bool SetMaxHDRate(int rate);

  private:
    static int funcIO(void *opaque, unsigned char *buf, int size);
    void Destroy();

  private:
    bool                m_complete {false};
    AVFormatContext    *m_oc       {nullptr};
    unsigned char       m_buffer[AudioOutput::kMaxSizeBuffer] {0};
    long                m_size     {0};
};

#endif
