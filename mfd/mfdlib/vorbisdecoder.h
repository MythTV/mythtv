#ifndef VORBISDECODER_H_
#define VORBISDECODER_H_
/*
	vorbisdecoder.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Header for vorbis decoder
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "decoder.h"

#include <vorbis/vorbisfile.h>


class VorbisDecoder : public Decoder
{
  public:
    VorbisDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~VorbisDecoder(void);

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

    OggVorbis_File oggfile;

    unsigned int bks;
    bool done, finish;
    long len, freq, bitrate;
    int chan;
    unsigned long output_size;
    double totalTime, seekTime;

    QString getComment(vorbis_comment *comment, const char *label);
};

#endif

