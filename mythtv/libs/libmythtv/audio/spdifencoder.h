#ifndef SPDIFENCODER_H_
#define SPDIFENCODER_H_

#include <cstddef>
#include <cstdint>

#include <QString>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

class SPDIFEncoder
{
  public:
    SPDIFEncoder(const QString& muxer, AVCodecID codec_id);
    ~SPDIFEncoder();
    void WriteFrame(unsigned char *data, int size);
    int  GetData(unsigned char *buffer, size_t &dest_size);
    int  GetProcessedSize();
    unsigned char *GetProcessedBuffer();
    void Reset();
    bool Succeeded() const  { return m_complete; };
    bool SetMaxHDRate(int rate);

  private:
    static int funcIO(void *opaque, const uint8_t *buf, int size);
    void Destroy();

  private:
    bool                m_complete {false};
    AVFormatContext    *m_oc       {nullptr};
    long                m_size     {0};
};

#endif
