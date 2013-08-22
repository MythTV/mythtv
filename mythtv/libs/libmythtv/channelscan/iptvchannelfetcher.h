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
    IPTVChannelInfo() {}
    IPTVChannelInfo(const QString &name,
                    const QString &xmltvid,
                    const QString &data_url,
                    uint data_bitrate,
                    const QString &fec_type,
                    const QString &fec_url0,
                    uint fec_bitrate0,
                    const QString &fec_url1,
                    uint fec_bitrate1) :
        m_name(name), m_xmltvid(xmltvid),
        m_tuning(data_url, data_bitrate,
                 fec_type, fec_url0, fec_bitrate0, fec_url1, fec_bitrate1)
    {
    }

    bool IsValid(void) const
    {
        return !m_name.isEmpty() && !m_tuning.IsValid();
    }

  public:
    QString m_name;
    QString m_xmltvid;
    IPTVTuningData m_tuning;
};
typedef QMap<QString,IPTVChannelInfo> fbox_chan_map_t;

class IPTVChannelFetcher : public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(IPTVChannelFetcher)

  public:
    IPTVChannelFetcher(uint cardid, const QString &inputname, uint sourceid,
                       ScanMonitor *monitor = NULL);
    ~IPTVChannelFetcher();

    void Scan(void);
    void Stop(void);

    static QString DownloadPlaylist(const QString &url, bool inQtThread);
    static fbox_chan_map_t ParsePlaylist(
        const QString &rawdata, IPTVChannelFetcher *fetcher = NULL);

  private:
    void SetTotalNumChannels(uint val) { _chan_cnt = (val) ? val : 1; }
    void SetNumChannelsParsed(uint);
    void SetNumChannelsInserted(uint);
    void SetMessage(const QString &status);

  protected:
    virtual void run(void); // QRunnable

  private:
    ScanMonitor *_scan_monitor;
    uint      _cardid;
    QString   _inputname;
    uint      _sourceid;
    uint      _chan_cnt;
    bool      _thread_running;
    bool      _stop_now;
    MThread  *_thread;
    QMutex    _lock;
};

#endif // _IPTVCHANNELFETCHER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
