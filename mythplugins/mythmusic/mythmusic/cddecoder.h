#ifndef CDDECODER_H_
#define CDDECODER_H_

#include "decoder.h"

#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}

class Metadata;

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

    Metadata *getMetadata(int track);
    Metadata *getMetadata();
    Metadata *getLastMetadata();
    void commitMetadata(Metadata *mdata);

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

