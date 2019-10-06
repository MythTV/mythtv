/** -*- Mode: c++ -*-
 *  IPTVChannelInfo
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTVCHANNELFETCHER_H_
#define _IPTVCHANNELFETCHER_H_

// Qt headers
#include <QString>
#include <QRunnable>
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QCoreApplication>

// MythTV headers
#include "iptvtuningdata.h"
#include "mthread.h"

class ScanMonitor;
class IPTVChannelFetcher;

class IPTVChannelInfo
{
    Q_DECLARE_TR_FUNCTIONS(IPTVChannelInfo)

  public:
    IPTVChannelInfo() = default;
    IPTVChannelInfo(const QString &name,
                    const QString &xmltvid,
                    const QString &data_url,
                    uint data_bitrate,
                    const QString &fec_type,
                    const QString &fec_url0,
                    uint fec_bitrate0,
                    const QString &fec_url1,
                    uint fec_bitrate1,
                    uint programnumber) :
        m_name(name), m_xmltvid(xmltvid), m_programNumber(programnumber),
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
    uint           m_programNumber {0};
    IPTVTuningData m_tuning;
};
typedef QMap<QString,IPTVChannelInfo> fbox_chan_map_t;

class IPTVChannelFetcher : public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(IPTVChannelFetcher);

  public:
    IPTVChannelFetcher(uint cardid, const QString &inputname, uint sourceid,
                       bool is_mpts, ScanMonitor *monitor = nullptr);
    ~IPTVChannelFetcher();

    void Scan(void);
    void Stop(void);
    fbox_chan_map_t GetChannels(void);

    static QString DownloadPlaylist(const QString &url);
    static fbox_chan_map_t ParsePlaylist(
        const QString &rawdata, IPTVChannelFetcher *fetcher = nullptr);

  private:
    void SetTotalNumChannels(uint val) { m_chan_cnt = (val) ? val : 1; }
    void SetNumChannelsParsed(uint);
    void SetNumChannelsInserted(uint);
    void SetMessage(const QString &status);

  protected:
    void run(void) override; // QRunnable

  private:
    ScanMonitor     *m_scan_monitor   {nullptr};
    uint             m_cardid;
    QString          m_inputname;
    uint             m_sourceid;
    bool             m_is_mpts;
    fbox_chan_map_t  m_channels;
    uint             m_chan_cnt       {1};
    bool             m_thread_running {false};
    bool             m_stop_now       {false};
    MThread         *m_thread         {nullptr};
    QMutex           m_lock;
};

#endif // _IPTVCHANNELFETCHER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
