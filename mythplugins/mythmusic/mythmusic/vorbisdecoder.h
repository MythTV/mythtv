#ifndef VORBISDECODER_H_
#define VORBISDECODER_H_

#include "decoder.h"

#include <vorbis/vorbisfile.h>

class Metadata;

class VorbisDecoder : public Decoder
{
  public:
    VorbisDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~VorbisDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

    Metadata *getMetadata();
    void commitMetadata(Metadata *mdata);

  private:
    void run();

    void flush(bool = FALSE);
    void deinit();

    bool inited, user_stop;
    int stat;
    char *output_buf;
    ulong output_bytes, output_at;

    OggVorbis_File oggfile;

    unsigned int bks;
    bool done, finish;
    long len, freq, bitrate;
    int chan;
    unsigned long output_size;
    double totalTime, seekTime;
};

#endif

