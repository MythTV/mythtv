#ifndef HDHRCHANNELFETCHER_H
#define HDHRCHANNELFETCHER_H

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
class HDHRChannelFetcher;

class HDHRChannelInfo
{
    Q_DECLARE_TR_FUNCTIONS(HDHRChannelInfo)

  public:
    HDHRChannelInfo() = default;
    HDHRChannelInfo(QString name,
                    QString number,
                    const QString& url,
                    QString modulation,
                    QString videoCodec,
                    QString audioCodec,
                    uint frequency,
                    uint serviceID,
                    uint networkID,
                    uint transportID):
        m_name(std::move(name)),
        m_number(std::move(number)),
        m_tuning(url, IPTVTuningData::http_ts),
        m_modulation(std::move(modulation)),
        m_videoCodec(std::move(videoCodec)),
        m_audioCodec(std::move(audioCodec)),
        m_frequency(frequency),
        m_serviceID(serviceID),
        m_networkID(networkID),
        m_transportID(transportID)
    {
        // Determine channel type from presence of audio and video codecs
        if (m_videoCodec.isEmpty())
        {
            if (m_audioCodec.isEmpty())
            {
                // No video, no audio, then it is Data
                m_channelType = "Data";
            }
            else
            {
                // Only audio channel then it is Radio
                m_channelType = "Radio";
            }
        }
        else
        {
            // Video with or without audio is always TV
            m_channelType = "TV";
        }
    }

    bool IsValid(void) const
    {
        return !m_name.isEmpty() && m_tuning.IsValid();
    }

  public:
    QString        m_name;
    QString        m_number;
    IPTVTuningData m_tuning;
    QString        m_channelType;     // TV/Radio/Data
    QString        m_modulation;
    QString        m_videoCodec;
    QString        m_audioCodec;
    uint           m_frequency   {0};
    uint           m_serviceID   {0};
    uint           m_networkID   {0};
    uint           m_transportID {0};
    bool           m_fta         {true};
};
using hdhr_chan_map_t = QMap<QString,HDHRChannelInfo>;

class HDHRChannelFetcher : public QRunnable
{
    Q_DECLARE_TR_FUNCTIONS(HDHRChannelFetcher);

  public:
    HDHRChannelFetcher(uint cardid, QString inputname, uint sourceid,
                       ServiceRequirements serviceType, ScanMonitor *monitor = nullptr);
    ~HDHRChannelFetcher() override;

    void Scan(void);
    void Stop(void);
    hdhr_chan_map_t GetChannels(void);

  private:
    void SetTotalNumChannels(uint val) { m_chanCnt = (val) ? val : 1; }
    void SetNumChannelsInserted(uint val);

  protected:
    void run(void) override; // QRunnable

  private:
    ScanMonitor         *m_scanMonitor    {nullptr};
    uint                 m_cardId;
    QString              m_inputName;
    uint                 m_sourceId;
    ServiceRequirements  m_serviceType;
    hdhr_chan_map_t     *m_channels       {nullptr};
    uint                 m_chanCnt        {1};
    bool                 m_threadRunning  {false};
    bool                 m_stopNow        {false};
    MThread             *m_thread         {nullptr};
    QMutex               m_lock;
};

#endif // HDHRCHANNELFETCHER_H
