#ifndef CDDECODER_H_
#define CDDECODER_H_

#include "decoder.h"

#include "config.h"

#ifdef HAVE_CDIO
# if __has_include(<cdio/paranoia/cdda.h>)
#  include <cdio/paranoia/cdda.h>
#  include <cdio/paranoia/paranoia.h>
# else
#  include <cdio/cdda.h>
#  include <cdio/paranoia.h>
# endif
#endif

#ifdef HAVE_MUSICBRAINZ
    #include "musicbrainz.h"
#endif // HAVE_MUSICBRAINZ

class MusicMetadata;

class CdDecoder : public Decoder
{
     Q_DECLARE_TR_FUNCTIONS(CdDecoder);

  public:
    CdDecoder(const QString &file, DecoderFactory *d, AudioOutput *o);
    ~CdDecoder() override;

    // Decoder implementation
    bool initialize() override; // Decoder
    void seek(double pos) override; // Decoder
    void stop() override; // Decoder

    MusicMetadata *getMetadata(void);

    // The following need to allocate a new MusicMetadata object each time,
    // because their callers free the returned value
    // TODO check this is still true
    MusicMetadata *getMetadata(int track);

    int getNumTracks();
    int getNumCDAudioTracks();

    void setDevice(const QString &dev);
    void setCDSpeed(int speed);

  private:
    void run() override; // MThread

    void writeBlock();
    void deinit();

    volatile bool      m_inited      {false};
    volatile bool      m_userStop    {false};

    QString            m_deviceName;

    static QRecursiveMutex& getCdioMutex();

    DecoderEvent::Type m_stat        {DecoderEvent::kError};
    char              *m_outputBuf   {nullptr};
    std::size_t        m_outputAt    {0};

    std::size_t        m_bks         {0};
    std::size_t        m_bksFrames   {0};
    std::size_t        m_decodeBytes {0};
    bool               m_finish      {false};
    long               m_freq        {0};
    long               m_bitrate     {0};
    int                m_chan        {0};
    double             m_seekTime    {-1.0};

    int                m_setTrackNum {-1};
    int                m_trackNum    {0};

#ifdef HAVE_CDIO
    CdIo_t            *m_cdio        {nullptr};
    cdrom_drive_t     *m_device      {nullptr};
    cdrom_paranoia_t  *m_paranoia    {nullptr};
    lsn_t              m_start       {CDIO_INVALID_LSN};
    lsn_t              m_end         {CDIO_INVALID_LSN};
    lsn_t              m_curPos      {CDIO_INVALID_LSN};
#endif

#ifdef HAVE_MUSICBRAINZ
    static MusicBrainz & getMusicBrainz();
#endif // HAVE_MUSICBRAINZ

};

#endif

