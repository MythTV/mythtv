// Std C headers
#include <cmath>
#include <unistd.h>
#include <utility>

// Qt headers
#include <QFile>
#include <QTextStream>

// MythTV headers
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "cardutil.h"
#include "channelutil.h"
#include "libmyth/mythcontext.h"
#include "recorders/vboxutils.h"
#include "scanmonitor.h"
#include "vboxchannelfetcher.h"

#define LOC QString("VBoxChanFetch: ")

VBoxChannelFetcher::VBoxChannelFetcher(uint cardid, QString inputname, uint sourceid,
                                       bool ftaOnly, ServiceRequirements serviceType,
                                       ScanMonitor *monitor) :
    m_scanMonitor(monitor),
    m_cardId(cardid),       m_inputName(std::move(inputname)),
    m_sourceId(sourceid),   m_ftaOnly(ftaOnly),
    m_serviceType(serviceType),
    m_thread(new MThread("VBoxChannelFetcher", this))
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Has ScanMonitor %1")
        .arg(monitor?"true":"false"));

    // videoDev is of the form xx.xx.xx.xx-n-t or xxxxxxx-n-t
    QString videoDev = CardUtil::GetVideoDevice(cardid);
    QStringList list = videoDev.split('-');
    if (list.count() == 3)
    {
        const QString& tunerType = list.at(2);
        if (tunerType == "DVBT")
            m_transType = "T";
        else if (tunerType == "DVBT/T2")
            m_transType = "T2";
        else if (tunerType == "DVBS")
            m_transType = "S";
        else if (tunerType == "DVBS/S2")
            m_transType = "S2";
        else if (tunerType == "DVBC")
            m_transType = "C";
        else if (tunerType == "ATSC")
            m_transType = "A";
    }

     LOG(VB_CHANNEL, LOG_INFO, LOC + QString("VideoDevice is: %1, tunerType is: %2")
         .arg(videoDev, m_transType));
}

VBoxChannelFetcher::~VBoxChannelFetcher()
{
    Stop();
    delete m_thread;
    m_thread = nullptr;
    delete m_channels;
    m_channels = nullptr;
}

/** \fn VBoxChannelFetcher::Stop(void)
 *  \brief Stops the scanning thread running
 */
void VBoxChannelFetcher::Stop(void)
{
    m_lock.lock();

    while (m_threadRunning)
    {
        m_stopNow = true;
        m_lock.unlock();
        m_thread->wait(5ms);
        m_lock.lock();
    }

    m_lock.unlock();

    m_thread->wait();
}

vbox_chan_map_t VBoxChannelFetcher::GetChannels(void)
{
    while (!m_thread->isFinished())
        m_thread->wait(500ms);

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Found %1 channels")
        .arg(m_channels->size()));
    return *m_channels;
}

void VBoxChannelFetcher::Scan(void)
{
    Stop();
    m_stopNow = false;
    m_thread->start();
}

void VBoxChannelFetcher::run(void)
{
    m_lock.lock();
    m_threadRunning = true;
    m_lock.unlock();

    // Step 1/3 : Get the IP of the VBox to query
    QString dev = CardUtil::GetVideoDevice(m_cardId);
    QString ip = VBox::getIPFromVideoDevice(dev);

    if (m_stopNow || ip.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Failed to get IP address from videodev (%1)").arg(dev));
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("VBox IP: %1").arg(ip));

    // Step 2/3 : Download
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanPercentComplete(5);
        m_scanMonitor->ScanAppendTextToLog(tr("Downloading Channel List"));
    }

    delete m_channels;

    VBox *vbox = new VBox(ip);
    m_channels = vbox->getChannels();
    delete vbox;

    if (m_stopNow || !m_channels)
    {
        if (!m_channels && m_scanMonitor)
        {
            m_scanMonitor->ScanAppendTextToLog(QCoreApplication::translate("(Common)", "Error"));
            m_scanMonitor->ScanPercentComplete(100);
            m_scanMonitor->ScanErrored(tr("Downloading Channel List Failed"));
        }
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    // Step 3/3 : Process
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanPercentComplete(35);
        m_scanMonitor->ScanAppendTextToLog(tr("Adding Channels"));
    }

    SetTotalNumChannels(m_channels->size());

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Found %1 channels")
        .arg(m_channels->size()));

    // add the channels to the DB
    vbox_chan_map_t::const_iterator it = m_channels->cbegin();
    for (uint i = 1; it != m_channels->cend(); ++it, ++i)
    {
        const QString& channum   = it.key();
        QString name      = (*it).m_name;
        QString xmltvid   = (*it).m_xmltvid.isEmpty() ? "" : (*it).m_xmltvid;
        uint serviceID    = (*it).m_serviceID;
        bool fta          = (*it).m_fta;
        QString chanType  = (*it).m_channelType;
        QString transType = (*it).m_transType;
        uint networkID    = (*it).m_networkID;
        uint transportID  = (*it).m_transportID;

        //: %1 is the channel number, %2 is the channel name
        QString msg = tr("Channel #%1 : %2").arg(channum, name);

        LOG(VB_CHANNEL, LOG_INFO, QString("Handling channel %1 %2")
            .arg(channum, name));
        uint mplexID = 0;
        if (m_ftaOnly && !fta)
        {
            // ignore this encrypted channel
            if (m_scanMonitor)
            {
                m_scanMonitor->ScanAppendTextToLog(tr("Ignoring Encrypted %1")
                                                   .arg(msg));
            }
        }
        else if (chanType == "Radio" && m_serviceType != kRequireAudio)
        {
            // ignore this radio channel
            if (m_scanMonitor)
            {
                m_scanMonitor->ScanAppendTextToLog(tr("Ignoring Radio %1")
                                                   .arg(msg));
            }
        }
        else if (!SupportedTransmission(transType))
        {
            // ignore this channel
            if (m_scanMonitor)
            {
                m_scanMonitor->ScanAppendTextToLog(tr("Ignoring Bad Transmission Type %1").arg(msg));
            }
        }
        else
        {
            int chanid = ChannelUtil::GetChanID(m_sourceId, channum);

            if (chanid <= 0)
            {
                if (m_scanMonitor)
                {
                    m_scanMonitor->ScanAppendTextToLog(tr("Adding %1").arg(msg));
                }
                chanid = ChannelUtil::CreateChanID(m_sourceId, channum);

                // mplexID will be created if necessary
                // inversion, bandwidth, transmission_mode, polarity, hierarchy, mod_sys and roll_off are given values, but not used
                // this is to ensure services API Channel/GetVideoMultiplexList returns a valid list
                mplexID = ChannelUtil::CreateMultiplex(m_sourceId, "dvb", 0, QString(), transportID, networkID, 0,
                                                       'a', 'v', 'a', 'a', QString(), QString(), 'a', QString(),
                                                       QString(), QString(), "UNDEFINED", "0.35");

                ChannelUtil::CreateChannel(mplexID, m_sourceId, chanid, name, name,
                                            channum, serviceID, 0, 0,
                                            false, kChannelVisible, QString(),
                                            QString(), "Default", xmltvid);

                ChannelUtil::CreateIPTVTuningData(chanid, (*it).m_tuning);
            }
            else
            {
                if (m_scanMonitor)
                {
                    m_scanMonitor->ScanAppendTextToLog(tr("Updating %1").arg(msg));
                }

                // mplexID will be created if necessary
                mplexID = ChannelUtil::CreateMultiplex(m_sourceId, "dvb", 0, QString(), transportID, networkID, 0,
                                                       'a', 'v', 'a', 'a', QString(), QString(), 'a', QString(),
                                                       QString(), QString(), "UNDEFINED", "0.35");

                // xmltvid parameter is set to null, user may have changed it, so do not overwrite as we are only updating
                ChannelUtil::UpdateChannel(mplexID, m_sourceId, chanid, name, name,
                                           channum, serviceID, 0, 0,
                                           false, kChannelVisible, QString(),
                                           QString(), "Default", QString());

                ChannelUtil::UpdateIPTVTuningData(chanid, (*it).m_tuning);
            }
        }

        SetNumChannelsInserted(i);
    }

    if (m_scanMonitor)
    {
        m_scanMonitor->ScanAppendTextToLog(tr("Done"));
        m_scanMonitor->ScanAppendTextToLog("");
        m_scanMonitor->ScanPercentComplete(100);
        m_scanMonitor->ScanComplete();
    }

    QMutexLocker locker(&m_lock);
    m_threadRunning = false;
    m_stopNow = true;
}

void VBoxChannelFetcher::SetNumChannelsInserted(uint val)
{
    uint minval = 70;
    uint range = 100 - minval;
    uint pct = minval + (uint) truncf((((float)val) / m_chanCnt) * range);
    if (m_scanMonitor)
        m_scanMonitor->ScanPercentComplete(pct);
}

bool VBoxChannelFetcher::SupportedTransmission(const QString& transType)
{
    if (transType == "UNKNOWN")
        return true;

    // both S and S2 tuners can tune an S channel
    if (transType == "S" && (m_transType == "S" || m_transType == "S2"))
        return true;

    // both T and T2 tuners can tune a T channel
    if (transType == "T" && (m_transType == "T" || m_transType == "T2"))
        return true;

    // for S2, T2, A and C the channel and tuner transmission types must match
    return transType == m_transType;
}
