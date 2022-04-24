#ifndef VBOXCHANNELFETCHER_H
#define VBOXCHANNELFETCHER_H

#include <utility>

// Qt headers
#include <QCoreApplication>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QRunnable>
#include <QString>

// MythTV headers
#include "libmythbase/mthread.h"
#include "libmythtv/iptvtuningdata.h"

#include "channelscantypes.h"

class ScanMonitor;
class VBoxChannelFetcher;

class VBoxChannelInfo
{
    Q_DECLARE_TR_FUNCTIONS(VBoxChannelInfo)

  public:
    VBoxChannelInfo() = default;
    VBoxChannelInfo(QString name,
                    QString xmltvid,
                    const QString &data_url,
                    bool fta,
                    QString chanType,
                    QString transType,
                    uint serviceID,
                    uint networkID,
                    uint transportID):
        m_name(std::move(name)), m_xmltvid(std::move(xmltvid)),
        m_serviceID(serviceID), m_fta(fta), m_channelType(std::move(chanType)),
        m_transType(std::move(transType)),
        m_tuning(data_url, IPTVTuningData::http_ts), m_networkID(networkID),
        m_transportID(transportID)
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
    ~VBoxChannelFetcher() override;

    void Scan(void);
    void Stop(void);
    vbox_chan_map_t GetChannels(void);

  private:
    void SetTotalNumChannels(uint val) { m_chanCnt = (val) ? val : 1; }
    void SetNumChannelsInserted(uint val);
    bool SupportedTransmission(const QString &transType);

  protected:
    void run(void) override; // QRunnable

  private:
    ScanMonitor         *m_scanMonitor    {nullptr};
    uint                 m_cardId;
    QString              m_inputName;
    uint                 m_sourceId;
    bool                 m_ftaOnly;
    ServiceRequirements  m_serviceType;
    QString              m_transType      {"UNKNOWN"};
    vbox_chan_map_t     *m_channels       {nullptr};
    uint                 m_chanCnt        {1};
    bool                 m_threadRunning  {false};
    bool                 m_stopNow        {false};
    MThread             *m_thread         {nullptr};
    QMutex               m_lock;
};

#endif // VBOXCHANNELFETCHER_H
