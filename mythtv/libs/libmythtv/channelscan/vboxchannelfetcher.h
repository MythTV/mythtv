#ifndef _VBOXCHANNELFETCHER_H_
#define _VBOXCHANNELFETCHER_H_

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
class VBoxChannelFetcher;

class VBoxChannelInfo
{
    Q_DECLARE_TR_FUNCTIONS(VBoxChannelInfo)

  public:
    VBoxChannelInfo() : m_serviceID(0) {}
    VBoxChannelInfo(const QString &name,
                    const QString &xmltvid,
                    const QString &data_url,
                    uint serviceID) :
        m_name(name), m_xmltvid(xmltvid), m_serviceID(serviceID),
        m_tuning(data_url, IPTVTuningData::http_ts)
    {
    }

    bool IsValid(void) const
    {
        return !m_name.isEmpty() && m_tuning.IsValid();
    }

  public:
    QString m_name;
    QString m_xmltvid;
    uint m_serviceID;
    IPTVTuningData m_tuning;
};
typedef QMap<QString,VBoxChannelInfo> vbox_chan_map_t;

class VBoxChannelFetcher : public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(VBoxChannelFetcher)

  public:
    VBoxChannelFetcher(uint cardid, const QString &inputname,
                       uint sourceid, ScanMonitor *monitor = NULL);
    ~VBoxChannelFetcher();

    void Scan(void);
    void Stop(void);
    vbox_chan_map_t GetChannels(void);

  private:
    void SetTotalNumChannels(uint val) { _chan_cnt = (val) ? val : 1; }
    void SetNumChannelsInserted(uint);

  protected:
    virtual void run(void); // QRunnable

  private:
    ScanMonitor *_scan_monitor;
    uint      _cardid;
    QString   _inputname;
    uint      _sourceid;
    vbox_chan_map_t *_channels;
    uint      _chan_cnt;
    bool      _thread_running;
    bool      _stop_now;
    MThread  *_thread;
    QMutex    _lock;
};

#endif // _VBOXCHANNELFETCHER_H_
