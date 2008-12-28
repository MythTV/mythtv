#ifndef AVFECODER_H_
#define AVFECODER_H_

#include "decoder.h"

extern "C" {
#include <mythtv/libavformat/avformat.h>
#include <mythtv/libavcodec/avcodec.h>
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

    void flush(bool = FALSE);
    void deinit();

    bool inited, user_stop;
    int stat;
    char *output_buf;
    ulong output_bytes, output_at;

    unsigned int bks;
    bool done, finish;
    long len, freq, bitrate;
    int m_channels;
    unsigned long output_size;
    double totalTime, seekTime;

    QString devicename;

    long int start;
    long int end;

    AVOutputFormat *m_outputFormat; // Encoding format (PCM)
    AVInputFormat *m_inputFormat; // Decoding format
    AVFormatParameters m_params;
    AVFormatParameters *m_ap;
    AVFormatContext *m_outputContext, *m_inputContext;
    AVStream *m_decStream;
    AVCodec *m_codec; // Codec
    AVCodecContext *m_audioEnc, *m_audioDec;
    AVPacket m_pkt1;
    AVPacket *m_pkt;

    int errcode;

    unsigned char *ptr;
    int dec_len, data_size;
    short samples[AVCODEC_MAX_AUDIO_FRAME_SIZE / 2];
    // short samples[(2 * 1024 * 32) / 2];
};

#endif

