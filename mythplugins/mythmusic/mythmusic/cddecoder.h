#ifndef CDDECODER_H_
#define CDDECODER_H_

#include "decoder.h"

#include <mythconfig.h>

#if CONFIG_DARWIN
#include <vector>
using std::vector;
#endif

#ifdef HAVE_CDIO
# include <cdio/cdda.h>
# include <cdio/paranoia.h>
#endif

class MusicMetadata;

class CdDecoder : public Decoder
{
  public:
    CdDecoder(const QString &file, DecoderFactory *, AudioOutput *);
    virtual ~CdDecoder();

    // Decoder implementation
    virtual bool initialize();
    virtual void seek(double);
    virtual void stop();

    // Decoder overrides
    virtual MusicMetadata *getMetadata(void);
    virtual void commitMetadata(MusicMetadata *mdata);

    // The following need to allocate a new MusicMetadata object each time,
    // because their callers free the returned value
    // TODO check this is still true
    MusicMetadata *getMetadata(int track);
    MusicMetadata *getLastMetadata();

#if CONFIG_DARWIN
    double lengthInSeconds();
#endif
    int getNumTracks();
    int getNumCDAudioTracks();

    void      setDevice(const QString &dev);
    void      setCDSpeed(int speed);

  private:
    void run();

    void writeBlock();
    void deinit();

    volatile bool m_inited;
    volatile bool m_user_stop;

    QString m_devicename;

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
    static QMutex& getCdioMutex();

    DecoderEvent::Type m_stat;
    char *m_output_buf;
    std::size_t m_output_at;

    std::size_t m_bks, m_bksFrames, m_decodeBytes;
    bool m_finish;
    long m_freq, m_bitrate;
    int m_chan;
    double m_seekTime;

    int m_settracknum;
    int m_tracknum;

#ifdef HAVE_CDIO
    CdIo_t *m_cdio;
    cdrom_drive_t *m_device;
    cdrom_paranoia_t *m_paranoia;
    lsn_t m_start, m_end, m_curpos;
#endif
};

#endif

