#ifndef AVFECODER_H_
#define AVFECODER_H_

#include <stdint.h>

#include "decoder.h"

#include <audiooutputsettings.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class Metadata;

class avfDecoder : public Decoder
{
  public:
    avfDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~avfDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

  private:
    void run();

    void writeBlock();
    void deinit();

    bool inited, user_stop;
    int stat;
    uint8_t *output_buf;
    ulong output_at;

    unsigned int bks, bksFrames, decodeBytes;
    bool finish;
    long freq, bitrate;
    AudioFormat m_sampleFmt;
    int m_channels;
    double seekTime;

    QString devicename;

    AVInputFormat *m_inputFormat;
    AVFormatContext *m_inputContext;
    AVCodec *m_codec; // Codec
    AVCodecContext *m_audioDec;

    bool           m_inputIsFile;
    unsigned char *m_buffer;
    AVIOContext *m_byteIOContext;

    int errcode;
};

#endif

