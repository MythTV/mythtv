#ifndef WAVDECODER_H_
#define WAVDECODER_H_
/*
	wavdecoder.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Header for wav decoder
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "decoder.h"

class WavDecoder : public Decoder
{
  public:

    WavDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~WavDecoder(void);

    bool   initialize();
    void   seek(double);
    void   stop();

    AudioMetadata *getMetadata();

  private:

    bool readWavHeader(QIODevice *the_input);
    uint getU32(char byte_zero, char byte_one, char byte_two, char byte_three);
    uint getU16(char byte_zero, char byte_one);
    void run();
    void flush(bool = FALSE);
    void deinit();

    bool inited;
    bool user_stop;

    int  stat;
    char *output_buf;
    ulong output_bytes, output_at;

    unsigned int bks;
    bool done, finish;
    long len, freq, bitrate, byte_rate, block_align, bits_per_sample, pcm_data_size;
    int chan;
    unsigned long output_size;
    double totalTime, seekTime;

};

#endif

