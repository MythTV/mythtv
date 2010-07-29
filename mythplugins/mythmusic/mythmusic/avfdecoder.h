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

    MetaIO *doCreateTagger(void);

  private:
    void run();

    void writeBlock();
    void deinit();

    bool inited, user_stop;
    int stat;
    char *output_buf;
    ulong output_at;

    unsigned int bks, bksFrames, decodeBytes;
    bool finish;
    long freq, bitrate;
    AudioFormat m_sampleFmt;
    int m_channels;
    double totalTime, seekTime;

    QString devicename;

    AVInputFormat *m_inputFormat;
    AVFormatParameters m_params;
    AVFormatContext *m_inputContext;
    AVStream *m_decStream;
    AVCodec *m_codec; // Codec
    AVCodecContext *m_audioDec;

    bool           m_inputIsFile;
    unsigned char *m_buffer;
    ByteIOContext *m_byteIOContext;

    int errcode;
    int16_t *m_samples;
};

#endif

