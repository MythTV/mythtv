#ifndef AVFECODER_H_
#define AVFECODER_H_


#include "../config.h"

#ifdef WMA_AUDIO_SUPPORT

#include "decoder.h"
#include <mythtv/ffmpeg/avformat.h>
#include <mythtv/ffmpeg/avcodec.h>


#
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

    AudioMetadata *getMetadata();

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
    int chan;
    unsigned long output_size;
    double totalTime, seekTime;

    QString devicename;

    long int start;
    long int end;

    AVOutputFormat *fmt;	// Encoding format (PCM)
    AVInputFormat *ifmt;	// Decoding format
    AVFormatParameters params;
    AVFormatParameters *ap;
    AVFormatContext *oc, *ic;
    AVStream *enc_st, *dec_st;
    AVCodec *codec, *enc_codec;		// Codec
    AVCodecContext *audio_enc, *audio_dec;
    AVPacket pkt1;
    AVPacket *pkt;

    int errcode;

    unsigned char *ptr;
    int dec_len, data_size;
    short samples[AVCODEC_MAX_AUDIO_FRAME_SIZE / 2];
    // short samples[(2 * 1024 * 32) / 2];
};

#endif
#endif

