// -*- Mode: c++ -*-
#ifndef _IPTV_TUNING_DATA_H_
#define _IPTV_TUNING_DATA_H_

// Qt headers
#include <QString>
#include <QUrl>

// MythTV headers
#include "mythtvexp.h"
#include "mythlogging.h"
#include "mythsingledownload.h"
#include "recorders/HLS/HLSReader.h"

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

    typedef enum IPTVProtocol
    {
        inValid = 0,
        udp,
        rtp,
        rtsp,
        http_ts,
        http_hls
    } IPTVProtocol;

    IPTVTuningData() : m_fec_type(kNone), m_protocol(inValid)
    {
        memset(&m_bitrate, 0, sizeof(m_bitrate));
    }

    IPTVTuningData(const QString &data_url, IPTVProtocol protocol) :
        m_data_url(data_url),  m_fec_type(kNone), m_protocol(protocol)
    {
        memset(&m_bitrate, 0, sizeof(m_bitrate));
    }

    IPTVTuningData(const QString &data_url, uint data_bitrate,
                   const FECType fec_type,
                   const QString &fec_url0, uint fec_bitrate0,
                   const QString &fec_url1, uint fec_bitrate1) :
        m_data_url(data_url),
        m_fec_type(fec_type), m_fec_url0(fec_url0), m_fec_url1(fec_url1)
    {
        GuessProtocol();
        m_bitrate[0] = data_bitrate;
        m_bitrate[1] = fec_bitrate0;
        m_bitrate[2] = fec_bitrate1;
    }

    IPTVTuningData(const QString &data_url, uint data_bitrate,
                   const QString &fec_type,
                   const QString &fec_url0, uint fec_bitrate0,
                   const QString &fec_url1, uint fec_bitrate1,
                   const IPTVProtocol protocol) :
        m_data_url(data_url),
        m_fec_type(kNone), m_fec_url0(fec_url0), m_fec_url1(fec_url1),
        m_protocol(protocol)
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
        if (IsHLS())
            return QString("%1(%2)").arg(u.toString()).arg(GetBitrate(0));
        if (IsHTTPTS() || IsRTSP())
            return QString("%1").arg(u.toString());
        return QString("%1:%2:%3")
            .arg(u.host()).arg(u.userInfo()).arg(u.port()).toLower();
    }

    QString GetDeviceName(void) const
    {
        return QString("[data]%1[fectype]%2[fec0]%3[fec1]%4%5")
            .arg(GetDataURL().toString())
            .arg(GetFECTypeString(1))
            .arg(GetFECURL1().toString())
            .arg(GetFECURL1().toString())
            .arg(GetBitrate(0) ? QString("-%1").arg(GetBitrate(0)) : "");
    }

    bool operator==(const IPTVTuningData &other) const
    {
        return GetDeviceName() == other.GetDeviceName();
    }

    bool operator!=(const IPTVTuningData &other) const
    {
        return GetDeviceName() != other.GetDeviceName();
    }

    void SetDataURL(const QUrl &url)
    {
        m_data_url = url;
        GuessProtocol();
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

    bool IsValid(void) const
    {
        bool ret = (m_data_url.isValid() && (IsUDP() || IsRTP() || IsRTSP() || IsHLS() || IsHTTPTS()));

        LOG(VB_CHANNEL, LOG_DEBUG, QString("IPTVTuningdata (%1): IsValid = %2")
            .arg(m_data_url.toString())
            .arg(ret ? "true" : "false"));

        return ret;
    }

    bool IsUDP(void) const
    {
        return (m_protocol == udp);
    }

    bool IsRTP(void) const
    {
        return (m_protocol == rtp);
    }

    bool IsRTSP(void) const
    {
        return (m_protocol == rtsp);
    }

    bool IsHLS() const
    {
        return (m_protocol == http_hls);
    }

    bool IsHTTPTS() const
    {
        return (m_protocol == http_ts);
    }

    void GuessProtocol(void)
    {
        if (!m_data_url.isValid())
            m_protocol = IPTVTuningData::inValid;
        else if (m_data_url.scheme() == "udp")
            m_protocol = IPTVTuningData::udp;
        else if (m_data_url.scheme() == "rtp")
            m_protocol = IPTVTuningData::rtp;
        else if (m_data_url.scheme() == "rtsp")
            m_protocol = IPTVTuningData::rtsp;
        else if (((m_data_url.scheme() == "http") || (m_data_url.scheme() == "https")) && IsHLSPlaylist())
            m_protocol = IPTVTuningData::http_hls;
        else if ((m_data_url.scheme() == "http") || (m_data_url.scheme() == "https"))
            m_protocol = IPTVTuningData::http_ts;
        else
            m_protocol = IPTVTuningData::inValid;
    }

  IPTVProtocol GetProtocol(void) const
  {
      return m_protocol;
  }

  protected:
    bool IsHLSPlaylist(void) const
    {
        if (!qApp)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("IsHLSPlaylist - No QApplication!!"));
            return false;
        }

        QString url = m_data_url.toString();

        // check url is valid for a playlist before downloading (see trac ticket #12856)
        if(url.endsWith(".m3u8", Qt::CaseInsensitive) ||
           url.endsWith(".m3u", Qt::CaseInsensitive))
        {
            LOG(VB_RECORD, LOG_INFO, QString("IsHLSPlaylist url ends with either .m3u8 or .m3u %1").arg(url));
        }
        else
        {
            // not a valid playlist so just return false
            LOG(VB_RECORD, LOG_INFO, QString("IsHLSPlaylist url does not end with either .m3u8 or .m3u %1").arg(url));
            return false;
        }

        QByteArray buffer;

        MythSingleDownload downloader;
        downloader.DownloadURL(url, &buffer, 5000, 0, 10000);
        if (!buffer.size())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("IsHLSPlaylist - Open Failed: %1\n\t\t\t%2")
                .arg(downloader.ErrorString()).arg(url));
            return false;
        }

        QTextStream text(&buffer);
        text.setCodec("UTF-8");
        return (HLSReader::IsValidPlaylist(text));
    }

  protected:
    QUrl         m_data_url;
    FECType      m_fec_type;
    QUrl         m_fec_url0;
    QUrl         m_fec_url1;
    uint         m_bitrate[3];
    IPTVProtocol m_protocol;
};

#endif // _IPTV_TUNING_DATA_H_
