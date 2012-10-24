// -*- Mode: c++ -*-
#ifndef _IPTV_TUNING_DATA_H_
#define _IPTV_TUNING_DATA_H_

// Qt headers
#include <QString>
#include <QUrl>

// MythTV headers
#include "mythtvexp.h"

class MTV_PUBLIC IPTVTuningData
{
  public:
    typedef enum FECType
    {
        kNone,
        kRFC2733,
        kRFC5109,
        kSMPTE2022,
    } FECType;

    typedef enum IPTVType
    {
        kData = 1,
        kRFC2733_1,
        kRFC2733_2,
        kRFC5109_1,
        kRFC5109_2,
        kSMPTE2022_1,
        kSMPTE2022_2,
    } IPTVType;

    IPTVTuningData() : m_fec_type(kNone)
    {
        memset(&m_bitrate, 0, sizeof(m_bitrate));
    }

    IPTVTuningData(const QString &data_url, uint data_bitrate,
                   FECType fec_type,
                   const QString &fec_url0, uint fec_bitrate0,
                   const QString &fec_url1, uint fec_bitrate1) :
        m_data_url(data_url),
        m_fec_type(fec_type), m_fec_url0(fec_url0), m_fec_url1(fec_url1)
    {
        m_bitrate[0] = data_bitrate;
        m_bitrate[1] = fec_bitrate0;
        m_bitrate[2] = fec_bitrate1;
    }

    IPTVTuningData(const QString &data_url, uint data_bitrate,
                   const QString &fec_type,
                   const QString &fec_url0, uint fec_bitrate0,
                   const QString &fec_url1, uint fec_bitrate1) :
        m_data_url(data_url),
        m_fec_type(kNone), m_fec_url0(fec_url0), m_fec_url1(fec_url1)
    {
        m_bitrate[0] = data_bitrate;
        m_bitrate[1] = fec_bitrate0;
        m_bitrate[2] = fec_bitrate1;
        if (fec_type.toLower() == "rfc2733")
            m_fec_type = kRFC2733;
        else if (fec_type.toLower() == "rfc5109")
            m_fec_type = kRFC5109;
        else if (fec_type.toLower() == "smpte2022")
            m_fec_type = kSMPTE2022;
        else
        {
            m_fec_url0.clear();
            m_fec_url1.clear();
        }
    }

    QString GetDeviceKey(void) const
    {
        const QUrl u = GetDataURL();
        return QString("%1:%2:%3")
            .arg(u.host()).arg(u.userInfo()).arg(u.port()).toLower();
    }

    QString GetDeviceName(void) const
    {
        return QString("[data]%1[fectype]%2[fec0]%3[fec1]%4")
            .arg(GetDataURL().toString())
            .arg(GetFECTypeString(1))
            .arg(GetFECURL1().toString())
            .arg(GetFECURL1().toString());
    }

    bool operator==(const IPTVTuningData &other) const
    {
        return GetDeviceName() == other.GetDeviceName();
    }

    bool operator!=(const IPTVTuningData &other) const
    {
        return GetDeviceName() != other.GetDeviceName();
    }

    QUrl GetDataURL(void) const { return m_data_url; }
    QUrl GetFECURL0(void) const { return m_fec_url0; }
    QUrl GetFECURL1(void) const { return m_fec_url1; }
    QUrl GetURL(uint i) const
    {
        if (0 == i)
            return GetDataURL();
        else if (1 == i)
            return GetFECURL0();
        else if (2 == i)
            return GetFECURL1();
        else
            return QUrl();
    }
    uint GetBitrate(uint i) const { return m_bitrate[i]; }

    FECType GetFECType(void) const { return m_fec_type; }

    QString GetFECTypeString(uint i) const
    {
        if (0 == i)
            return "data";
        switch (m_fec_type)
        {
            case kNone: return QString();
            case kRFC2733: return QString("rfc2733-%1").arg(i);
            case kRFC5109: return QString("rfc5109-%1").arg(i);
            case kSMPTE2022: return QString("smpte2022-%1").arg(i);
        }
        return QString();
    }

    uint GetURLCount(void) const { return 3; }

    bool IsValid(void) const { return m_data_url.port() != -1; }

  public:
    QUrl m_data_url;
    FECType m_fec_type;
    QUrl m_fec_url0;
    QUrl m_fec_url1;
    uint m_bitrate[3];
};

#endif // _IPTV_TUNING_DATA_H_
