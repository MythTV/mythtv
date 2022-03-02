#ifndef HLS_SEGMENT_H
#define HLS_SEGMENT_H

#include <cstdint>

#include <QString>
#include <QUrl>

#include "libmythbase/mythchrono.h"

class HLSRecSegment
{
  public:
    friend class HLSReader;

    HLSRecSegment(void);
    HLSRecSegment(const HLSRecSegment& rhs);
    HLSRecSegment(int seq, std::chrono::seconds duration, QString title,
		  QUrl uri);
    HLSRecSegment(int seq, std::chrono::seconds duration, QString title,
                  QUrl uri, const QString& current_key_path);
    ~HLSRecSegment();

    HLSRecSegment& operator=(const HLSRecSegment& rhs);

    bool Download(QByteArray & buffer);

    int64_t Sequence(void) const { return m_sequence; }
    QString Title(void)    const { return m_title; }
    QUrl    Url(void)      const { return m_url; }
    std::chrono::seconds Duration(void) const { return m_duration; }

    QString toString(void) const;

#ifdef USING_LIBCRYPTO
    bool DownloadKey(void);
    bool DecodeData(const uint8_t *IV, QByteArray& data);
    bool HasKeyPath(void) const { return !m_psz_key_path.isEmpty(); }
    QString KeyPath(void) const { return m_psz_key_path; }
    void SetKeyPath(const QString& path) { m_psz_key_path = path; }
#endif

  protected:
    int64_t     m_sequence {0};  // unique sequence number
    std::chrono::seconds m_duration {0s};  // segment duration
    uint64_t    m_bitrate  {0};  // bitrate of segment's content (bits per second)
    QString     m_title;         // human-readable informative title of
			         // the media segment

    QUrl        m_url;

#ifdef USING_LIBCRYPTO
    QString     m_psz_key_path;  // URL key path
#endif

};

#endif // HLS_SEGMENT_H
