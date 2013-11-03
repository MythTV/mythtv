#include "HLSSegment.h"
#include "HLSReader.h"

#define LOC QString("HLSSegment: ")

HLSRecSegment::HLSRecSegment(void)
    : m_sequence(0),
      m_duration(0),
      m_bitrate(0)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecSegment::HLSRecSegment(const HLSRecSegment& rhs)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
    operator=(rhs);
}

HLSRecSegment::HLSRecSegment(int seq, int duration,
                       const QString& title, const QString& uri)
    : m_sequence(seq),
      m_duration(duration),
      m_bitrate(0),
      m_title(title),
      m_url(uri)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecSegment::HLSRecSegment(int seq, int duration, const QString& title,
           const QString& uri, const QString current_key_path)
    : m_sequence(seq),
      m_duration(duration),
      m_bitrate(0),
      m_title(title),
      m_url(uri)
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
