/** -*- Mode: c++ -*-
 *  IPTVChannelInfo
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef IPTVCHANNELFETCHER_H
#define IPTVCHANNELFETCHER_H

#include <utility>

// Qt headers
#include <QCoreApplication>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QRunnable>
#include <QString>

// MythTV headers
#include "libmythtv/iptvtuningdata.h"
#include "libmythbase/mthread.h"

class ScanMonitor;
class IPTVChannelFetcher;

class IPTVChannelInfo
{
    Q_DECLARE_TR_FUNCTIONS(IPTVChannelInfo)

  public:
    IPTVChannelInfo() = default;
    IPTVChannelInfo(QString name,
                    QString xmltvid,
                    QString logo,
                    const QString &data_url,
                    uint data_bitrate,
                    const QString &fec_type,
                    const QString &fec_url0,
                    uint fec_bitrate0,
                    const QString &fec_url1,
                    uint fec_bitrate1,
                    uint programnumber) :
        m_name(std::move(name)), m_xmltvid(std::move(xmltvid)), m_logo(std::move(logo)),
        m_programNumber(programnumber),
        m_tuning(data_url, data_bitrate,
                 fec_type, fec_url0, fec_bitrate0, fec_url1, fec_bitrate1,
                 IPTVTuningData::inValid)
    {
    }

 protected:
    friend class TestIPTVRecorder;
    bool IsValid(void) const
    {
        return !m_name.isEmpty() && m_tuning.IsValid();
    }

  public:
    QString        m_name;
    QString        m_xmltvid;
    QString        m_logo;
    uint           m_programNumber {0};
    IPTVTuningData m_tuning;
};
using fbox_chan_map_t = QMap<QString,IPTVChannelInfo>;

class IPTVChannelFetcher : public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(IPTVChannelFetcher);

  public:
    IPTVChannelFetcher(uint cardid, QString inputname, uint sourceid,
                       bool is_mpts, ScanMonitor *monitor = nullptr);
    ~IPTVChannelFetcher() override;

    void Scan(void);
    void Stop(void);
    fbox_chan_map_t GetChannels(void);

    static QString DownloadPlaylist(const QString &url);
    MTV_PUBLIC static fbox_chan_map_t ParsePlaylist(
        const QString &rawdata, IPTVChannelFetcher *fetcher = nullptr);

  private:
    void SetTotalNumChannels(uint val) { m_chanCnt = (val) ? val : 1; }
    void SetNumChannelsParsed(uint val);
    void SetNumChannelsInserted(uint val);
    void SetMessage(const QString &status);

  protected:
    void run(void) override; // QRunnable

  private:
    ScanMonitor     *m_scanMonitor    {nullptr};
    uint             m_cardId;
    QString          m_inputName;
    uint             m_sourceId;
    bool             m_isMpts;
    fbox_chan_map_t  m_channels;
    uint             m_chanCnt        {1};
    bool             m_threadRunning  {false};
    bool             m_stopNow        {false};
    MThread         *m_thread         {nullptr};
    QMutex           m_lock;
};

#endif // IPTVCHANNELFETCHER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
