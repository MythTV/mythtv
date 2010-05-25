#ifndef CDDECODER_H_
#define CDDECODER_H_

#include "decoder.h"

#include <mythconfig.h>

#if CONFIG_DARWIN
#include <vector>
#endif

#if defined(__linux__) || defined(__FreeBSD__)
#include <cdaudio.h>
extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}
#endif

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

    // The following need to allocate a new Metadata object each time,
    // because their callers (e.g. databasebox.cpp) free the returned value
    Metadata *getMetadata(int track);
    Metadata *getMetadata(void);
    Metadata *getLastMetadata(void);

    void commitMetadata(Metadata *mdata);
    void      setDevice(const QString &dev)  { devicename = dev; }
    void      setCDSpeed(int speed);

  private:
    void run();

    void writeBlock(void);
    void deinit();

    bool inited, user_stop;

    QString devicename;

#if CONFIG_DARWIN
    void lookupCDDB(const QString &hexID, uint tracks);

    uint32_t           m_diskID;        ///< For CDDB1/FreeDB lookup
    uint               m_firstTrack,    ///< First AUDIO track
                       m_lastTrack,     ///< Last  AUDIO track
                       m_leadout;       ///< End of last track
    double             m_lengthInSecs;
    vector<int>        m_tracks;        ///< Start block offset of each track
    vector<Metadata*>  m_mData;         ///< After lookup, details of each trk
#endif

    int stat;
    char *output_buf;
    ulong output_at;

    unsigned int bks, bksFrames, decodeBytes;
    bool finish;
    long freq, bitrate;
    int chan;
    double totalTime, seekTime;

    int settracknum;
    int tracknum;

#if defined(__linux__) || defined(__FreeBSD__)
    cdrom_drive *device;
    cdrom_paranoia *paranoia;
#endif

    long int start, end, curpos;
};

#endif

