#ifndef AVFECODER_H_
#define AVFECODER_H_
/*
	aacdecoder.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Header for aac (m4a) decoder
	
*/



#include "../config.h"

#ifdef AAC_AUDIO_SUPPORT

#include <qsqldatabase.h>

#include "decoder.h"

#include <mp4ff.h>


class Metadata;

class aacDecoder : public Decoder
{
  public:
    aacDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~aacDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

    AudioMetadata *getMetadata();

    bool     initializeMP4();
    int      getAACTrack(mp4ff_t *infile);
    uint32_t aacRead(char *buffer, uint32_t length);
    uint32_t aacSeek(uint64_t position);

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
    long len, bitrate;
    uchar channels;
    unsigned long sample_rate;
    unsigned long output_size;
    double totalTime, seekTime;

    bool mp4_file_flag;
    mp4ff_callback_t *mp4_callback;
    faacDecHandle decoder_handle;
    mp4ff_t *mp4_input_file;
    int aac_track_number;
    unsigned long timescale;
    unsigned int framesize;
};

#endif
#endif

