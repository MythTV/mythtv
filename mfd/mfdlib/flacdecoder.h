#ifndef FLACDECODER_H_
#define FLACDECODER_H_
/*
	flacdecoder.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Header for flac decoder
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "decoder.h"

#include <FLAC/all.h>

//  class Metadata;

class FlacDecoder : public Decoder
{
  public:
    FlacDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~FlacDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

    void doWrite(const FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
    void setFlacMetadata(const FLAC__StreamMetadata *metadata);
    void flacError(FLAC__StreamDecoderErrorStatus status);
    AudioMetadata *getMetadata();
    //  void commitMetadata(Metadata *mdata);

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
    //  void setComment(FLAC__StreamMetadata *block, const char *label,
    //                const QString &data);
};

#endif

