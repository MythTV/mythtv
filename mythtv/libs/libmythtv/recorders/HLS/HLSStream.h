#ifndef HLS_STREAM_
#define HLS_STREAM_

#include <QMap>
#include <QQueue>

#include "libmythbase/mythconfig.h"
#include "libmythbase/mythsingledownload.h"
#include "HLSSegment.h"

// 128-bit AES key for HLS segment decryption
static constexpr uint8_t AES128_KEY_SIZE { 16 };
struct hls_aes_key_st {
    std::array<uint8_t,AES128_KEY_SIZE> key;
};
using HLS_AES_KEY = struct hls_aes_key_st;

class HLSRecStream
{
    friend class HLSReader;

  public:
#if CONFIG_LIBCRYPTO
    using AESKeyMap = QMap<QString, HLS_AES_KEY* >;
#endif  // CONFIG_LIBCRYPTO

    HLSRecStream(int inputId, int seq, uint64_t bitrate, QString m3u8_url, QString segment_base_url);
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
    void SetDateTime(QDateTime &dt) { m_dateTime = dt; }
    bool Live(void) const           { return m_live; }
    void SetLive(bool x)            { m_live = x; }
    QString M3U8Url(void) const     { return m_m3u8Url; }
    QString SegmentBaseUrl(void) const { return m_segmentBaseUrl; }
    void SetSegmentBaseUrl(const QString &n) { m_segmentBaseUrl = n; }
    QString MapUri(void) const       { return m_mapUri; }
    void SetMapUri(const QString& x) { m_mapUri = x; }

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

#if CONFIG_LIBCRYPTO
  protected:
    int Decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
                unsigned char *iv, unsigned char *plaintext) const;
    bool DownloadKey(MythSingleDownload& downloader,
          const QString& keypath, HLS_AES_KEY* aeskey) const;
    bool DecodeData(MythSingleDownload& downloader,
		    const QByteArray& IV, const QString& keypath,
		    QByteArray& data, int64_t sequence);
#endif // CONFIG_LIBCRYPTO

  protected:
    void AverageBandwidth(int64_t bandwidth);

  private:
    int         m_inputId        {0};     // input card ID
    int         m_id;                     // program id
    int         m_version        {1};     // HLS protocol version
    std::chrono::seconds m_targetDuration {-1s}; // maximum duration per segment
    uint64_t    m_curByteRate    {0};
    uint64_t    m_bitrate        {0};     // bitrate of stream content (bits per second)
    std::chrono::seconds m_duration {0s}; // duration of the stream
    int         m_discontSeq     {0};     // discontinuity sequence number
    bool        m_live           {true};
    int64_t     m_bandwidth      {0};     // measured average download bandwidth (bits/second)
    double      m_sumBandwidth   {0.0};
    QQueue<int64_t> m_bandwidthSegs;

    QString     m_m3u8Url ;               // uri to m3u8
    QString     m_segmentBaseUrl;         // uri to base for relative segments (m3u8 redirect target)
    mutable QMutex  m_lock;
    bool        m_cache          {false}; // allow caching
    QDateTime   m_dateTime;               // #EXT-X-PROGRAM-DATE-TIME
    int         m_retries        {0};

    QString     m_mapUri;                 // URI of Media Initialisation Sequence

#if CONFIG_LIBCRYPTO
  private:
    AESKeyMap   m_aesKeys;                // AES-128 keys by path
#endif // CONFIG_LIBCRYPTO
};

#endif // HLS_STREAM_H
