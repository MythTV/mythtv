#ifndef AVFECODER_H_
#define AVFECODER_H_

#include <stdint.h>

#include "decoder.h"

#include <audiooutputsettings.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class avfDecoder : public Decoder
{
  public:
    avfDecoder(const QString &file, DecoderFactory *, AudioOutput *);
    virtual ~avfDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

  private:
    void run();

    void deinit();

    bool m_inited, m_userStop;
    int m_stat;
    uint8_t *m_outputBuffer;

    bool m_finish;
    long m_freq, m_bitrate;
    int m_channels;
    double m_seekTime;

    QString m_devicename;

    AVInputFormat *m_inputFormat;
    AVFormatContext *m_inputContext;
    AVCodecContext *m_audioDec;

    bool m_inputIsFile;
    AVIOContext *m_byteIOContext;

    int m_errCode;
};

#endif

