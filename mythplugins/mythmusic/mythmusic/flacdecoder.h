#ifndef VORBISDECODER_H_
#define VORBISDECODER_H_

#include "decoder.h"

#include <FLAC/all.h>

class Metadata;

class FlacDecoder : public Decoder
{
  public:
    FlacDecoder(MythContext *context, const QString &file, DecoderFactory *, 
                QIODevice *, Output *);
    virtual ~FlacDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

    void doWrite(const FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
    void setFlacMetadata(const FLAC__StreamMetadata *metadata);

    Metadata *getMetadata(QSqlDatabase *db);
    void commitMetadata(Metadata *mdata);

  private:
    void run();

    void flush(bool = FALSE);
    void deinit();

    bool inited, user_stop;
    int stat;
    char *output_buf;
    ulong output_bytes, output_at;

    FLAC__SeekableStreamDecoder *decoder;

    unsigned int bks;
    bool done, finish;
    long len, freq, bitrate;
    int chan;
    int bitspersample;
    unsigned long output_size;
    double totalTime, seekTime;
    unsigned long totalsamples;

    QString getComment(FLAC__StreamMetadata *block, const char *label);
    void setComment(FLAC__StreamMetadata *block, const char *label,
                    const QString &data);
};

#endif

