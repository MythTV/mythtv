#ifndef _HLS_Stream_h_
#define _HLS_Stream_h_

#ifdef USING_LIBCRYPTO
// encryption related stuff
#include <openssl/aes.h>
#define AES_BLOCK_SIZE 16       // HLS only support AES-128
#endif // USING_LIBCRYPTO

#include <QMap>

#include "mythsingledownload.h"
#include "HLSSegment.h"

class HLSRecStream
{
    friend class HLSReader;

  public:
#ifdef USING_LIBCRYPTO
    typedef QMap<QString, AES_KEY* >  AESKeyMap;
#endif

    HLSRecStream(int seq, uint64_t bitrate, const QString& uri);
    ~HLSRecStream(void);

    int Read(uint8_t* buffer, int len);

    QString toString(void) const;
    int Id(void) const              { return m_id; }
    int Version(void) const         { return m_version; }
    void SetVersion(int x)          { m_version = x; }
    int TargetDuration(void) const  { return m_targetduration; }
    void SetTargetDuration(int x)   { m_targetduration = x; }
    uint64_t AverageBandwidth(void) { return m_bandwidth; }
    uint64_t Bitrate(void) const    { return m_bitrate; }
    void SetBitrate(uint64_t bitrate) { m_bitrate = bitrate; }
    uint64_t CurrentByteRate(void) const { return m_curbyterate; }
    void SetCurrentByteRate(uint64_t byterate) { m_curbyterate = byterate; }
    bool Cache(void) const          { return m_cache; }
    void SetCache(bool x)           { m_cache = x; }
    bool Live(void) const           { return m_live; }
    void SetLive(bool x)            { m_live = x; }
    QString Url(void) const         { return m_url; }

    uint Duration(void) const;
    uint NumCachedSegments(void) const;
    uint NumReleasedSegments(void) const;
    uint NumTotalSegments(void) const;

    static bool IsGreater(const HLSRecStream *s1, const HLSRecStream *s2);
    bool operator<(const HLSRecStream &b) const;
    bool operator>(const HLSRecStream &b) const;

#ifdef USING_LIBCRYPTO
  protected:
    bool DownloadKey(MythSingleDownload& downloader,
		     const QString& keypath, AES_KEY* aeskey);
    bool DecodeData(MythSingleDownload& downloader,
		    const uint8_t *IV, const QString& keypath,
		    QByteArray& data, int64_t sequence);
    bool SetAESIV(QString line);
    bool IVLoaded(void) const { return m_ivloaded; }

    uint8_t *AESIV(void) { return m_AESIV; }
    void SetKeyPath(const QString x) { m_keypath = x; }
#endif // USING_LIBCRYPTO

  protected:
    void AverageBandwidth(int64_t bandwidth);

  private:
    int         m_id;              // program id
    int         m_version;         // protocol version should be 1
    int         m_targetduration;  // maximum duration per segment (s)
    uint64_t    m_curbyterate;
    uint64_t    m_bitrate;         // bitrate of stream content (bits per second)
    int64_t     m_duration;        // duration of the stream in seconds
    bool        m_live;
    int64_t     m_bandwidth; // measured average download bandwidth (bits/second)
    double      m_sumbandwidth;
    int         m_countbandwidth;

    QString     m_url;             // uri to m3u8
    mutable QMutex  m_lock;
    bool        m_cache;           // allow caching

#ifdef USING_LIBCRYPTO
  private:
    QString     m_keypath;              // URL path of the encrypted key
    bool        m_ivloaded;
    uint8_t     m_AESIV[AES_BLOCK_SIZE];// IV used when decypher the block
    AESKeyMap   m_aeskeys;       // AES-128 keys by path
#endif // USING_LIBCRYPTO
};

#endif
