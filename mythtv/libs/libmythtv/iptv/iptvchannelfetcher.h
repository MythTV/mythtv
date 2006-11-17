#ifndef _IPTVCHANNELFETCHER_H_
#define _IPTVCHANNELFETCHER_H_

// POSIX headers
#include <pthread.h>

// Qt headers
#include <qobject.h>
#include <qmutex.h>

// MythTV headers
#include "iptvchannelinfo.h"

class IPTVChannelFetcher : public QObject
{
    Q_OBJECT

    friend void *run_scan_thunk(void *param);

  public:
    IPTVChannelFetcher(uint cardid, const QString &inputname, uint sourceid);

    bool Scan(void);
    void Stop(void);

    static QString DownloadPlaylist(const QString &url, bool inQtThread);
    static fbox_chan_map_t ParsePlaylist(
        const QString &rawdata, IPTVChannelFetcher *fetcher = NULL);

  signals:
    /** \brief Tells listener how far along we are from 0..100%
     *  \param p percentage completion
     */
    void ServiceScanPercentComplete(int p);
    /// \brief Returns tatus message from the scanner
    void ServiceScanUpdateText(const QString &status);
    /// \brief Signals that the scan is complete
    void ServiceScanComplete(void);

  private:
    ~IPTVChannelFetcher();
    void SetTotalNumChannels(uint val) { _chan_cnt = (val) ? val : 1; }
    void SetNumChannelsParsed(uint);
    void SetNumChannelsInserted(uint);
    void SetMessage(const QString &status);
    void RunScan(void);

  private:
    uint      _cardid;
    QString   _inputname;
    uint      _sourceid;
    uint      _chan_cnt;
    bool      _thread_running;
    bool      _stop_now;
    pthread_t _thread;
    QMutex    _lock;
};

#endif // _IPTVCHANNELFETCHER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
