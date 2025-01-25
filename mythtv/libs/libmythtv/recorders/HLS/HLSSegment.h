#ifndef HLS_SEGMENT_H
#define HLS_SEGMENT_H

#include <cstdint>

#ifdef USING_LIBCRYPTO
// encryption related stuff
#include <openssl/aes.h>
#endif // USING_LIBCRYPTO

#include <QString>
#include <QUrl>

#include "libmythbase/mythchrono.h"
#include "libmythbase/mythsingledownload.h"

class HLSRecSegment
{
  public:
    friend class HLSReader;

    HLSRecSegment(void);
    HLSRecSegment(const HLSRecSegment& rhs);
    HLSRecSegment(int seq, std::chrono::milliseconds duration, QString title,
		  QUrl uri);
    ~HLSRecSegment();

    HLSRecSegment& operator=(const HLSRecSegment& rhs);

    bool Download(QByteArray & buffer);

    int64_t Sequence(void) const { return m_sequence; }
    QString Title(void)    const { return m_title; }
    QUrl    Url(void)      const { return m_url; }
    std::chrono::milliseconds Duration(void) const { return m_duration; }

    QString toString(void) const;

#ifdef USING_LIBCRYPTO
  public:
    bool SetAESIV(QString line);
    bool IVLoaded(void) const { return m_ivLoaded; }

    QByteArray AESIV(void) { return m_aesIV; }
    bool HasKeyPath(void) const { return !m_keypath.isEmpty(); }
    QString KeyPath(void) const { return m_keypath; }
    void SetKeyPath(const QString& x) { m_keypath = x; }
#endif // USING_LIBCRYPTO

  protected:
    int64_t     m_sequence {0};                 // unique sequence number
    std::chrono::milliseconds m_duration {0ms}; // segment duration
    uint64_t    m_bitrate  {0};                 // bitrate of segment's content (bits per second)
    QString     m_title;                        // human-readable informative title of the media segment
    QUrl        m_url;

#ifdef USING_LIBCRYPTO
  private:
    QString     m_keypath;                      // URL path of the encrypted key
    bool        m_ivLoaded  {false};
    QByteArray  m_aesIV     {AES_BLOCK_SIZE,0}; // IV used when decypher the block
#endif // USING_LIBCRYPTO

};

#endif // HLS_SEGMENT_H
