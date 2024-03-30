// -*- Mode: c++ -*-

// Std C headers
#include <cmath>
#include <unistd.h>
#include <utility>

// Qt headers
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

// MythTV headers
#include "libmyth/mythcontext.h"
#include "libmythbase/mythlogging.h"

#include "cardutil.h"
#include "channelutil.h"
#include "externrecscanner.h"
#include "recorders/ExternalRecChannelFetcher.h"
#include "scanmonitor.h"

#define LOC QString("ExternRecChanFetch: ")

ExternRecChannelScanner::ExternRecChannelScanner(uint cardid,
                                                 QString inputname,
                                                 uint sourceid,
                                                 ScanMonitor *monitor)
    : m_scanMonitor(monitor)
    , m_cardId(cardid)
    , m_inputName(std::move(inputname))
    , m_sourceId(sourceid)
    , m_thread(new MThread("ExternRecChannelScanner", this))
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Has ScanMonitor %1")
        .arg(monitor ? "true" : "false"));
}

ExternRecChannelScanner::~ExternRecChannelScanner(void)
{
    Stop();
    delete m_thread;
    m_thread = nullptr;
}

/** \fn ExternRecChannelScanner::Stop(void)
 *  \brief Stops the scanning thread running
 */
void ExternRecChannelScanner::Stop(void)
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

/// \brief Scans the list.
void ExternRecChannelScanner::Scan(void)
{
    Stop();
    m_stopNow = false;
    m_thread->start();
}

void ExternRecChannelScanner::run(void)
{
    m_lock.lock();
    m_threadRunning = true;
    m_lock.unlock();


    // Step 1/4 : Get info from DB
    QString cmd = CardUtil::GetVideoDevice(m_cardId);

    if (m_stopNow || cmd.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Invalid external command");
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("External Command: '%1'").arg(cmd));


    // Step 2/4 : Download
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanPercentComplete(1);
        m_scanMonitor->ScanAppendTextToLog(tr("Creating channel list"));
    }

    ExternalRecChannelFetcher fetch(m_cardId, cmd);

    m_channelTotal = fetch.LoadChannels();
    if (m_channelTotal < 1)
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "Failed to load channels");
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    std::vector<uint> existing = ChannelUtil::GetChanIDs(m_sourceId);
    std::vector<uint>::iterator Iold;

    // Step 3/4 : Process
    if (m_scanMonitor)
        m_scanMonitor->ScanAppendTextToLog(tr("Processing channels"));

    QString channum;
    QString name;
    QString callsign;
    QString xmltvid;
    QString icon;
    int     atsc_major = 0;
    int     atsc_minor = 0;
    int     cnt = 0;

    if (!fetch.FirstChannel(channum, name, callsign, xmltvid, icon))
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + "No channels found.");
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    if (m_scanMonitor)
        m_scanMonitor->ScanAppendTextToLog(tr("Adding Channels"));

    uint idx = 0;
    for (;;)
    {
        static const QRegularExpression digitRE { "\\D+" };
        QString msg = tr("Channel #%1 : %2").arg(channum, name);
        QStringList digits = channum.split(digitRE);

        if (digits.size() > 1)
        {
            atsc_major = digits.at(0).toInt();
            atsc_minor = digits.at(1).toInt();
            LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                QString("ATSC: %1.%2").arg(atsc_major).arg(atsc_minor));
        }
        else
        {
            atsc_major = atsc_minor = 0;
        }

        LOG(VB_CHANNEL, LOG_INFO, QString("Handling channel %1 %2")
            .arg(channum, name));

        int chanid = ChannelUtil::GetChanID(m_sourceId, channum);

        if (m_scanMonitor)
            m_scanMonitor->ScanPercentComplete(++cnt * 100 / m_channelTotal);

        if (chanid <= 0)
        {
            if (m_scanMonitor)
                m_scanMonitor->ScanAppendTextToLog(tr("Adding %1").arg(msg));

            chanid = ChannelUtil::CreateChanID(m_sourceId, channum);
            ChannelUtil::CreateChannel(0, m_sourceId, chanid, callsign, name,
                                       channum, 1, atsc_major, atsc_minor,
                                       false, kChannelVisible, QString(),
                                       icon, "Default", xmltvid);
        }
        else
        {
            if (m_scanMonitor)
                m_scanMonitor->ScanAppendTextToLog(tr("Updating %1").arg(msg));

            ChannelUtil::UpdateChannel(0, m_sourceId, chanid, callsign, name,
                                       channum, 1, atsc_major, atsc_minor,
                                       false, kChannelVisible, QString(),
                                       icon, "Default", xmltvid);
        }

        SetNumChannelsInserted(cnt);

        Iold = std::find(existing.begin(), existing.end(), chanid);
        if (Iold != existing.end())
        {
            existing.erase(Iold);
        }

        if (++idx < m_channelTotal)
            fetch.NextChannel(channum, name, callsign, xmltvid, icon);
        else
            break;
    }

    // Remove any channels which are no longer valid
    for (Iold = existing.begin(); Iold != existing.end(); ++Iold)
    {
        channum = ChannelUtil::GetChanNum(*Iold);

        if (m_scanMonitor)
            m_scanMonitor->ScanAppendTextToLog
                (tr("Removing unused Channel #%1").arg(channum));

        ChannelUtil::DeleteChannel(*Iold);
    }

    // Step 4/4 : Finish up
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Found %1 channels").arg(cnt));
    if (m_scanMonitor)
        m_scanMonitor->ScanAppendTextToLog
            (tr("Found %1 channels.").arg(cnt));

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

void ExternRecChannelScanner::SetNumChannelsParsed(uint val)
{
    uint minval = 35;
    uint range = 70 - minval;
    uint pct = minval + (uint) truncf((((float)val) / m_channelCnt) * range);
    if (m_scanMonitor)
        m_scanMonitor->ScanPercentComplete(pct);
}

void ExternRecChannelScanner::SetNumChannelsInserted(uint val)
{
    uint minval = 70;
    uint range = 100 - minval;
    uint pct = minval + (uint) truncf((((float)val) / m_channelCnt) * range);
    if (m_scanMonitor)
        m_scanMonitor->ScanPercentComplete(pct);
}

void ExternRecChannelScanner::SetMessage(const QString &status)
{
    if (m_scanMonitor)
        m_scanMonitor->ScanAppendTextToLog(status);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
