#ifndef VORBISDECODER_H_
#define VORBISDECODER_H_

#include "decoder.h"

#include <cdaudio.h>

class Metadata;

class CdDecoder : public Decoder
{
  public:
    CdDecoder(const QString &file, DecoderFactory *, QIODevice *, Output *);
    virtual ~CdDecoder(void);

    bool initialize();
    double lengthInSeconds();
    void seek(double);
    void stop();

    int getNumTracks(void);

    Metadata *getMetadata(QSqlDatabase *db, int track);
    Metadata *getMetadata(QSqlDatabase *db);
    void commitMetadata(Metadata *mdata);

  private:
    void run();

    void deinit();

    bool inited, user_stop;
    int stat;

    unsigned int bks;
    bool done, finish;
    long len, freq, bitrate;
    int chan;
    unsigned long output_size;
    double totalTime, seekTime;

    int cd_desc;
    int tracknum;

    QString devicename;

    int settracknum;
};

#endif

