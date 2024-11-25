// -*- Mode: c++ -*-
#ifndef IPTV_TUNING_DATA_H
#define IPTV_TUNING_DATA_H

#include <array>

// Qt headers
#include <QApplication>
#include <QString>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif
#include <QUrl>

// MythTV headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsingledownload.h"
#include "libmythtv/mythtvexp.h"
#include "libmythtv/recorders/HLS/HLSReader.h"

class MTV_PUBLIC IPTVTuningData
{
  public:
    enum FECType : std::uint8_t
    {
        kNone,
        kRFC2733,
        kRFC5109,
        kSMPTE2022,
    };

    enum IPTVType : std::uint8_t
    {
        kData = 1,
        kRFC2733_1,
        kRFC2733_2,
        kRFC5109_1,
        kRFC5109_2,
        kSMPTE2022_1,
        kSMPTE2022_2,
    };

    enum IPTVProtocol : std::uint8_t
    {
        inValid = 0,
        udp,
        rtp,
        rtsp,
        http_ts,
        http_hls
    };

    IPTVTuningData()
    {
        m_bitrate.fill(0);
    }

    IPTVTuningData(const QString &data_url, IPTVProtocol protocol) :
        m_dataUrl(data_url),  m_protocol(protocol)
    {
        m_bitrate.fill(0);
    }

    IPTVTuningData(const QString &data_url, uint data_bitrate,
                   const FECType fec_type,
                   const QString &fec_url0, uint fec_bitrate0,
                   const QString &fec_url1, uint fec_bitrate1) :
        m_dataUrl(data_url),
        m_fecType(fec_type), m_fecUrl0(fec_url0), m_fecUrl1(fec_url1)
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
        m_dataUrl(data_url),
        m_fecUrl0(fec_url0), m_fecUrl1(fec_url1),
        m_protocol(protocol)
    {
        m_bitrate[0] = data_bitrate;
        m_bitrate[1] = fec_bitrate0;
        m_bitrate[2] = fec_bitrate1;
        if (fec_type.toLower() == "rfc2733")
            m_fecType = kRFC2733;
        else if (fec_type.toLower() == "rfc5109")
            m_fecType = kRFC5109;
        else if (fec_type.toLower() == "smpte2022")
            m_fecType = kSMPTE2022;
        else
        {
            m_fecUrl0.clear();
            m_fecUrl1.clear();
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
            .arg(u.host(),u.userInfo(),QString::number(u.port())).toLower();
    }

    QString GetDeviceName(void) const
    {
        return QString("[data]%1[fectype]%2[fec0]%3[fec1]%4%5")
            .arg(GetDataURL().toString(),
                 GetFECTypeString(1),
                 GetFECURL1().toString(),
                 GetFECURL1().toString(),
                 GetBitrate(0) ? QString("-%1").arg(GetBitrate(0)) : "");
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
        m_dataUrl = url;
        GuessProtocol();
    }

    QUrl GetDataURL(void) const { return m_dataUrl; }
    QUrl GetFECURL0(void) const { return m_fecUrl0; }
    QUrl GetFECURL1(void) const { return m_fecUrl1; }
    QUrl GetURL(uint i) const
    {
        if (0 == i)
            return GetDataURL();
        if (1 == i)
            return GetFECURL0();
        if (2 == i)
            return GetFECURL1();
        return {};
    }
    uint GetBitrate(uint i) const { return m_bitrate[i]; }

    FECType GetFECType(void) const { return m_fecType; }

    QString GetFECTypeString(uint i) const
    {
        if (0 == i)
            return "data";
        switch (m_fecType)
        {
            case kNone: return {};
            case kRFC2733: return QString("rfc2733-%1").arg(i);
            case kRFC5109: return QString("rfc5109-%1").arg(i);
            case kSMPTE2022: return QString("smpte2022-%1").arg(i);
        }
        return {};
    }

    static uint GetURLCount(void) { return 3; }

    bool IsValid(void) const
    {
        bool ret = (m_dataUrl.isValid() && (IsUDP() || IsRTP() || IsRTSP() || IsHLS() || IsHTTPTS()));

        LOG(VB_CHANNEL, LOG_DEBUG, QString("IPTVTuningdata (%1): IsValid = %2")
            .arg(m_dataUrl.toString(),
                 ret ? "true" : "false"));

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
        if (!m_dataUrl.isValid())
            m_protocol = IPTVTuningData::inValid; // NOLINT(bugprone-branch-clone)
        else if (m_dataUrl.scheme() == "udp")
            m_protocol = IPTVTuningData::udp;
        else if (m_dataUrl.scheme() == "rtp")
            m_protocol = IPTVTuningData::rtp;
        else if (m_dataUrl.scheme() == "rtsp")
            m_protocol = IPTVTuningData::rtsp;
        else if (((m_dataUrl.scheme() == "http") || (m_dataUrl.scheme() == "https")) && IsHLSPlaylist())
            m_protocol = IPTVTuningData::http_hls;
        else if ((m_dataUrl.scheme() == "http") || (m_dataUrl.scheme() == "https"))
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
        if (QCoreApplication::instance() == nullptr)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("IsHLSPlaylist - No QCoreApplication!!"));
            return false;
        }

        MythSingleDownload downloader;
        QString url = m_dataUrl.toString();
        QByteArray buffer;
        downloader.DownloadURL(url, &buffer, 5s, 0, 10000);
        if (buffer.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("IsHLSPlaylist - Open Failed:%1 url:%2")
                .arg(downloader.ErrorString(), url));
            return false;
        }

        QTextStream text(&buffer);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        text.setCodec("UTF-8");
#else
        text.setEncoding(QStringConverter::Utf8);
#endif
        return (HLSReader::IsValidPlaylist(text));
    }

  protected:
    QUrl         m_dataUrl;
    FECType      m_fecType    {kNone};
    QUrl         m_fecUrl0;
    QUrl         m_fecUrl1;
    std::array<uint,3> m_bitrate {};
    IPTVProtocol m_protocol   {inValid};
};

#endif // IPTV_TUNING_DATA_H
