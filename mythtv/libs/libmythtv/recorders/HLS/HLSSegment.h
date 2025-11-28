#ifndef HLS_SEGMENT_H
#define HLS_SEGMENT_H

#include "libmythbase/mythconfig.h"

#include <cstdint>

#if CONFIG_LIBCRYPTO
#include <openssl/aes.h>
#endif // CONFIG_LIBCRYPTO

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "libmythbase/mythchrono.h"

class HLSRecSegment
{
  public:
    friend class HLSReader;

    HLSRecSegment(int input);
    HLSRecSegment(const HLSRecSegment& rhs);
    HLSRecSegment(int input, int sequence, std::chrono::milliseconds duration,
                  QString title, QUrl uri);
    ~HLSRecSegment();

    HLSRecSegment& operator=(const HLSRecSegment& rhs);

    bool Download(QByteArray & buffer);

    int64_t Sequence(void) const { return m_sequence; }
    QString Title(void)    const { return m_title; }
    QUrl    Url(void)      const { return m_url; }
    std::chrono::milliseconds Duration(void) const { return m_duration; }

    QString toString(void) const;

#if CONFIG_LIBCRYPTO
  public:
    bool SetAESIV(QString line);
    bool IVLoaded(void) const { return m_ivLoaded; }

    QByteArray AESIV(void) { return m_aesIV; }
    bool HasKeyPath(void) const { return !m_keypath.isEmpty(); }
    QString KeyPath(void) const { return m_keypath; }
    void SetKeyPath(const QString& x) { m_keypath = x; }
#endif // CONFIG_LIBCRYPTO

  protected:
    int         m_inputId  {0};                 // input card ID
    int64_t     m_sequence {0};                 // unique sequence number
    std::chrono::milliseconds m_duration {0ms}; // segment duration
    uint64_t    m_bitrate  {0};                 // bitrate of segment's content (bits per second)
    QString     m_title;                        // human-readable informative title of the media segment
    QUrl        m_url;

#if CONFIG_LIBCRYPTO
  private:
    QString     m_keypath;                      // URL path of the encrypted key
    bool        m_ivLoaded  {false};
    QByteArray  m_aesIV     {AES_BLOCK_SIZE,0}; // IV used when decypher the block
#endif // CONFIG_LIBCRYPTO

};

#endif // HLS_SEGMENT_H
