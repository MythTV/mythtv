#ifndef CDDECODER_H_
#define CDDECODER_H_
/*
	cddecoder.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Header for cd decoder
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "decoder.h"

#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}

//  class Metadata;

class CdDecoder : public Decoder
{
  public:
    CdDecoder(const QString &file, DecoderFactory *, QIODevice *, AudioOutput *);
    virtual ~CdDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

    int getNumTracks(void);
    int getNumCDAudioTracks(void);

    AudioMetadata* getMetadata(int track);
    AudioMetadata* getMetadata();

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

    int settracknum;
    int tracknum;

    cdrom_drive *device;
    cdrom_paranoia *paranoia;

    long int start;
    long int end;
    long int curpos;
};

#endif

