#ifndef MADDECODER_H_
#define MADDECODER_H_
/*
	maddecoder.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Header for mad mp3 decoder
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "decoder.h"

extern "C" {
#include <mad.h>
}


class MadDecoder : public Decoder
{
  public:
    MadDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~MadDecoder(void);

    bool initialize();
    void seek(double);
    void stop();

    static const int maxDecodeRetries;
    static const int maxFrameSize;
    static const int maxFrameCheck;
    static const int initialFrameSize;

    AudioMetadata *getMetadata();

private:
    void run();

    enum mad_flow madOutput();
    enum mad_flow madError(struct mad_stream *, struct mad_frame *);

    void flush(bool = FALSE);
    void deinit();
    bool findHeader();
    bool findXingHeader(struct mad_bitptr, unsigned int);
    void calcLength(struct mad_header *);

    bool inited, user_stop, done, finish, derror, eof, useeq;
    double totalTime, seekTime;
    int stat, channels;
    long bitrate, freq, len;
    unsigned int bks;
    mad_fixed_t eqbands[32];

    char *input_buf;
    unsigned long input_bytes;

    char *output_buf;
    unsigned long output_bytes, output_at, output_size;

    struct {
        int flags;
        unsigned long frames;
        unsigned long bytes;
        unsigned char toc[100];
        long scale;
    } xing;

    enum {
        XING_FRAMES = 0x0001,
        XING_BYTES  = 0x0002,
        XING_TOC    = 0x0004,
        XING_SCALE  = 0x0008
    };

    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
};

#endif

