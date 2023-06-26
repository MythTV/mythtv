#include "HLSSegment.h"

// C/C++
#include <utility>

#include "HLSReader.h"

#define LOC QString("HLSSegment: ")

HLSRecSegment::HLSRecSegment(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecSegment::HLSRecSegment(const HLSRecSegment& rhs)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
    operator=(rhs);
}

HLSRecSegment::HLSRecSegment(int seq, std::chrono::seconds duration,
                       QString title, QUrl uri)
    : m_sequence(seq),
      m_duration(duration),
      m_title(std::move(title)),
      m_url(std::move(uri))
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecSegment::HLSRecSegment(int seq, std::chrono::seconds duration, QString title,
           QUrl uri, [[maybe_unused]] const QString& current_key_path)
    : m_sequence(seq),
      m_duration(duration),
      m_title(std::move(title)),
      m_url(std::move(uri))
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
#ifdef USING_LIBCRYPTO
    m_psz_key_path  = current_key_path;
#endif
}

HLSRecSegment& HLSRecSegment::operator=(const HLSRecSegment& rhs)
{
    if (&rhs != this)
    {
        m_sequence = rhs.m_sequence;
        m_duration = rhs.m_duration;
        m_bitrate = rhs.m_bitrate;
        m_title = rhs.m_title;
        m_url = rhs.m_url;
#ifdef USING_LIBCRYPTO
        m_psz_key_path  = rhs.m_psz_key_path;
#endif
    }
    return *this;
}

HLSRecSegment::~HLSRecSegment(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "dtor");
}

QString HLSRecSegment::toString(void) const
{
    return QString("[%1] '%2' @ '%3' for %4")
        .arg(m_sequence).arg(m_title, m_url.toString(), QString::number(m_duration.count()));
}
