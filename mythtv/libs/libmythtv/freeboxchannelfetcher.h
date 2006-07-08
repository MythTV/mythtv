#ifndef _FREEBOXCHANNELFETCHER_H_
#define _FREEBOXCHANNELFETCHER_H_

// POSIX headers
#include <pthread.h>

// Qt headers
#include <qobject.h>
#include <qmutex.h>

// MythTV headers
#include "freeboxchannelinfo.h"

class FreeboxChannelFetcher : public QObject
{
    Q_OBJECT

    friend void *run_scan_thunk(void *param);

  public:
    FreeboxChannelFetcher(unsigned _sourceid, unsigned _cardid);
    ~FreeboxChannelFetcher();

    /// Stops the scanning thread running
    void Stop(void);
    /// Scans the given frequency list, implies starting the thread()
    bool Scan(void);

    static QString DownloadPlaylist(const QString &url, bool inQtThread);
    static fbox_chan_map_t ParsePlaylist(
        const QString &rawdata, FreeboxChannelFetcher *fetcher = NULL);

  signals:
    /** \brief Tells listener how far along we are from 0..100%
     *  \param p percentage completion
     */
    void ServiceScanPercentComplete(int p);
    /** \brief Status message from the scanner
     *  \param status The latest status message
     */ 
    void ServiceScanUpdateText(const QString &status);
    /// Signals that the scan is complete
    void ServiceScanComplete(void);

  private:
    void SetTotalNumChannels(uint val) { chan_cnt = (val) ? val : 1; }
    void SetNumChannelsParsed(uint);
    void SetNumChannelsInserted(uint);
    void SetMessage(const QString &status);
    void RunScan(void);

    uint      sourceid;
    uint      cardid;
    uint      chan_cnt;
    bool      thread_running;
    bool      stop_now;
    pthread_t thread;
    QMutex    lock;
};

#endif //_FREEBOXCHANNELFETCHER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
