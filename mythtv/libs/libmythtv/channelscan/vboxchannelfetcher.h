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
#include "channelscantypes.h"

class ScanMonitor;
class VBoxChannelFetcher;

class VBoxChannelInfo
{
    Q_DECLARE_TR_FUNCTIONS(VBoxChannelInfo)

  public:
    VBoxChannelInfo() = default;
    VBoxChannelInfo(const QString &name,
                    const QString &xmltvid,
                    const QString &data_url,
                    bool fta,
                    const QString &chanType,
                    const QString &transType,
                    uint serviceID,
                    uint networkID,
                    uint transportID):
        m_name(name), m_xmltvid(xmltvid), m_serviceID(serviceID),
        m_fta(fta), m_channelType(chanType), m_transType(transType),
        m_tuning(data_url, IPTVTuningData::http_ts), m_networkID(networkID), m_transportID(transportID)
    {
    }

    bool IsValid(void) const
    {
        return !m_name.isEmpty() && m_tuning.IsValid();
    }

  public:
    QString        m_name;
    QString        m_xmltvid;
    uint           m_serviceID   {0};
    bool           m_fta         {false};
    QString        m_channelType;     // TV/Radio
    QString        m_transType;       // T/T2/S/S2/C/A
    IPTVTuningData m_tuning;
    uint           m_networkID   {0}; // Network ID from triplet
    uint           m_transportID {0}; // Transport ID from triplet
};
using vbox_chan_map_t = QMap<QString,VBoxChannelInfo>;

class VBoxChannelFetcher : public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(VBoxChannelFetcher);

  public:
    VBoxChannelFetcher(uint cardid, QString inputname, uint sourceid,
                       bool ftaOnly, ServiceRequirements serviceType,
                       ScanMonitor *monitor = nullptr);
    ~VBoxChannelFetcher();

    void Scan(void);
    void Stop(void);
    vbox_chan_map_t GetChannels(void);

  private:
    void SetTotalNumChannels(uint val) { m_chan_cnt = (val) ? val : 1; }
    void SetNumChannelsInserted(uint);
    bool SupportedTransmission(const QString &transType);

  protected:
    void run(void) override; // QRunnable

  private:
    ScanMonitor         *m_scan_monitor   {nullptr};
    uint                 m_cardid;
    QString              m_inputname;
    uint                 m_sourceid;
    bool                 m_ftaOnly;
    ServiceRequirements  m_serviceType;
    QString              m_transType      {"UNKNOWN"};
    vbox_chan_map_t     *m_channels       {nullptr};
    uint                 m_chan_cnt       {1};
    bool                 m_thread_running {false};
    bool                 m_stop_now       {false};
    MThread             *m_thread         {nullptr};
    QMutex               m_lock;
};

#endif // _VBOXCHANNELFETCHER_H_
