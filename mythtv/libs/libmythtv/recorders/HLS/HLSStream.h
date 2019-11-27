#ifndef _HLS_Stream_h_
#define _HLS_Stream_h_

#ifdef USING_LIBCRYPTO
// encryption related stuff
#include <openssl/aes.h>
#define AES_BLOCK_SIZE 16       // HLS only support AES-128
#endif // USING_LIBCRYPTO

#include <QMap>
#include <QQueue>

#include "mythsingledownload.h"
#include "HLSSegment.h"

class HLSRecStream
{
    friend class HLSReader;

  public:
#ifdef USING_LIBCRYPTO
    using AESKeyMap = QMap<QString, AES_KEY* >;
#endif

    HLSRecStream(int seq, uint64_t bitrate, QString m3u8_url, QString segment_base_url);
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
    QString M3U8Url(void) const     { return m_m3u8_url; }
    QString SegmentBaseUrl(void) const { return m_segment_base_url; }
    void SetSegmentBaseUrl(QString &n) { m_segment_base_url = n; }

    uint Duration(void) const;
    uint NumCachedSegments(void) const;
    uint NumReleasedSegments(void) const;
    uint NumTotalSegments(void) const;

    void Good(void) { m_retries = 0; }
    void Retrying(void) { ++m_retries; }
    int  RetryCount(void) const { return m_retries; }

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
    int         m_id;                     // program id
    int         m_version        {1};     // protocol version should be 1
    int         m_targetduration {-1};    // maximum duration per segment (s)
    uint64_t    m_curbyterate    {0};
    uint64_t    m_bitrate;                // bitrate of stream content (bits per second)
    int64_t     m_duration       {0LL};   // duration of the stream in seconds
    bool        m_live           {true};
    int64_t     m_bandwidth      {0};     // measured average download bandwidth (bits/second)
    double      m_sumbandwidth   {0.0};
    QQueue<int64_t> m_bandwidth_segs;

    QString     m_m3u8_url;               // uri to m3u8
    QString     m_segment_base_url;       // uri to base for relative segments (m3u8 redirect target)
    mutable QMutex  m_lock;
    bool        m_cache          {false}; // allow caching
    int         m_retries        {0};

#ifdef USING_LIBCRYPTO
  private:
    QString     m_keypath;              // URL path of the encrypted key
    bool        m_ivloaded       {false};
    uint8_t     m_AESIV[AES_BLOCK_SIZE] {0};// IV used when decypher the block
    AESKeyMap   m_aeskeys;       // AES-128 keys by path
#endif // USING_LIBCRYPTO
};

#endif
