#ifndef HLS_STREAM_
#define HLS_STREAM_

#ifdef USING_LIBCRYPTO
// encryption related stuff
#include <openssl/aes.h>
#endif // USING_LIBCRYPTO

#include <QMap>
#include <QQueue>

#include "libmythbase/mythsingledownload.h"
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
    std::chrono::seconds TargetDuration(void) const  { return m_targetDuration; }
    void SetTargetDuration(std::chrono::seconds x)   { m_targetDuration = x; }
    int DiscontinuitySequence(void)       { return m_discontSeq; }
    void SetDiscontinuitySequence(int s)  { m_discontSeq = s; }
    uint64_t AverageBandwidth(void) const { return static_cast<uint64_t>(m_bandwidth); }
    uint64_t Bitrate(void) const    { return m_bitrate; }
    void SetBitrate(uint64_t bitrate) { m_bitrate = bitrate; }
    uint64_t CurrentByteRate(void) const { return m_curByteRate; }
    void SetCurrentByteRate(uint64_t byterate) { m_curByteRate = byterate; }
    bool Cache(void) const          { return m_cache; }
    void SetCache(bool x)           { m_cache = x; }
    bool Live(void) const           { return m_live; }
    void SetLive(bool x)            { m_live = x; }
    QString M3U8Url(void) const     { return m_m3u8Url; }
    QString SegmentBaseUrl(void) const { return m_segmentBaseUrl; }
    void SetSegmentBaseUrl(const QString &n) { m_segmentBaseUrl = n; }

    std::chrono::seconds Duration(void) const;
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
		    const QByteArray& IV, const QString& keypath,
		    QByteArray& data, int64_t sequence);
    bool SetAESIV(QString line);
    bool IVLoaded(void) const { return m_ivLoaded; }

    QByteArray AESIV(void) { return m_aesIV; }
    void SetKeyPath(const QString& x) { m_keypath = x; }
#endif // USING_LIBCRYPTO

  protected:
    void AverageBandwidth(int64_t bandwidth);

  private:
    int         m_id;                     // program id
    int         m_version        {1};     // protocol version should be 1
    std::chrono::seconds m_targetDuration {-1s}; // maximum duration per segment
    uint64_t    m_curByteRate    {0};
    uint64_t    m_bitrate;                // bitrate of stream content (bits per second)
    std::chrono::seconds m_duration {0s};   // duration of the stream
    int         m_discontSeq     {0};     // Discontinuity sequence number
    bool        m_live           {true};
    int64_t     m_bandwidth      {0};     // measured average download bandwidth (bits/second)
    double      m_sumBandwidth   {0.0};
    QQueue<int64_t> m_bandwidthSegs;

    QString     m_m3u8Url ;               // uri to m3u8
    QString     m_segmentBaseUrl;         // uri to base for relative segments (m3u8 redirect target)
    mutable QMutex  m_lock;
    bool        m_cache          {false}; // allow caching
    int         m_retries        {0};

#ifdef USING_LIBCRYPTO
  private:
    QString     m_keypath;              // URL path of the encrypted key
    bool        m_ivLoaded       {false};
    QByteArray  m_aesIV          {AES_BLOCK_SIZE,0};// IV used when decypher the block
    AESKeyMap   m_aesKeys;       // AES-128 keys by path
#endif // USING_LIBCRYPTO
};

#endif // HLS_STREAM_H
