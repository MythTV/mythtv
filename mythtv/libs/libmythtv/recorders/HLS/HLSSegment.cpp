#include "HLSSegment.h"

// C/C++
#include <utility>

#if CONFIG_LIBCRYPTO
#include <openssl/aes.h>
#endif // CONFIG_LIBCRYPTO

#include "libmythbase/mythlogging.h"

#define LOC QString("HLSSegment[%1]: ").arg(m_inputId)

HLSRecSegment::HLSRecSegment(int inputId)
    : m_inputId(inputId)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecSegment::HLSRecSegment(const HLSRecSegment& rhs)
{
    operator=(rhs);
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecSegment::HLSRecSegment(int inputId, int sequence,
                             std::chrono::milliseconds duration,
                             QString title, QUrl uri)
    : m_inputId(inputId),
      m_sequence(sequence),
      m_duration(duration),
      m_title(std::move(title)),
      m_url(std::move(uri))
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "ctor");
}

HLSRecSegment& HLSRecSegment::operator=(const HLSRecSegment& rhs)
{
    if (&rhs != this)
    {
        m_inputId = rhs.m_inputId;
        m_sequence = rhs.m_sequence;
        m_duration = rhs.m_duration;
        m_bitrate = rhs.m_bitrate;
        m_title = rhs.m_title;
        m_url = rhs.m_url;
#if CONFIG_LIBCRYPTO
        m_keypath  = rhs.m_keypath;
        m_ivLoaded = rhs.m_ivLoaded;
        m_aesIV = rhs.m_aesIV;
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

#if CONFIG_LIBCRYPTO
bool HLSRecSegment::SetAESIV(QString line)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "SetAESIV line:"+ line);

    /*
     * If the EXT-X-KEY tag has the IV attribute, implementations MUST use
     * the attribute value as the IV when encrypting or decrypting with that
     * key.  The value MUST be interpreted as a 128-bit hexadecimal number
     * and MUST be prefixed with 0x or 0X.
     */
    if (!line.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SetAESIV does not start with 0x");
        return false;
    }

    if (line.size() % 2)
    {
        // not even size, pad with front 0
        line.insert(2, QLatin1String("0"));
    }
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    int padding = std::max(0, AES_BLOCK_SIZE - (line.size() - 2));
#else
    int padding = std::max(static_cast<qsizetype>(0), AES_BLOCK_SIZE - (line.size() - 2));
#endif
    QByteArray ba = QByteArray(padding, 0x0);
    ba.append(QByteArray::fromHex(QByteArray(line.toLatin1().constData() + 2)));
    m_aesIV = ba;
    m_ivLoaded = true;
    return true;
}
#endif // CONFIG_LIBCRYPTO
