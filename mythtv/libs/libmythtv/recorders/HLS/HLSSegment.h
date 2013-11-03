#ifndef _HLS_Segment_h_
#define _HLS_Segment_h_

#include <stdint.h>

#include <QString>

class HLSRecSegment
{
  public:
    friend class HLSReader;

    HLSRecSegment(void);
    HLSRecSegment(const HLSRecSegment& rhs);
    HLSRecSegment(int seq, int duration, const QString& title,
		  const QString& uri);
    HLSRecSegment(int seq, int duration, const QString& title,
	       const QString& uri, const QString current_key_path);
    ~HLSRecSegment();

    HLSRecSegment& operator=(const HLSRecSegment& rhs);

    bool Download(QByteArray & buffer);

    int64_t Sequence(void) const       { return m_sequence; }
    QString Title(void) const { return m_title; }
    QString Url(void) const  { return m_url; }
    int Duration(void) const { return m_duration; }

#ifdef USING_LIBCRYPTO
    bool DownloadKey(void);
    bool DecodeData(const uint8_t *IV, QByteArray& data);
    bool HasKeyPath(void) const { return !m_psz_key_path.isEmpty(); }
    QString KeyPath(void) const { return m_psz_key_path; }
    void SetKeyPath(const QString path) { m_psz_key_path = path; }
#endif

  protected:
    int64_t    m_sequence;   // unique sequence number
    int         m_duration;  // segment duration (seconds)
    uint64_t    m_bitrate;   // bitrate of segment's content (bits per second)
    QString     m_title;     // human-readable informative title of
			     // the media segment

    QString     m_url;

#ifdef USING_LIBCRYPTO
    QString     m_psz_key_path; // URL key path
#endif

};

#endif
