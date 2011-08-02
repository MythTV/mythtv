#ifndef _IPTVCHANNELFETCHER_H_
#define _IPTVCHANNELFETCHER_H_

// Qt headers
#include <QRunnable>
#include <QObject>
#include <QMutex>

// MythTV headers
#include "iptvchannelinfo.h"
#include "mthread.h"

class ScanMonitor;
class IPTVChannelFetcher;

class IPTVChannelFetcher : public QRunnable
{
  public:
    IPTVChannelFetcher(uint cardid, const QString &inputname, uint sourceid,
                       ScanMonitor *monitor = NULL);
    ~IPTVChannelFetcher();

    bool Scan(void);
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
