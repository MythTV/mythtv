// -*- Mode: c++ -*-

// Std C headers
#include <cmath>
#include <unistd.h>
#include <utility>

// Qt headers
#include <QFile>
#include <QTextStream>

// MythTV headers
#include "mythcontext.h"
#include "cardutil.h"
#include "channelutil.h"
#include "externrecscanner.h"
#include "scanmonitor.h"
#include "mythlogging.h"
#include "ExternalRecChannelFetcher.h"

#define LOC QString("ExternRecChanFetch: ")

ExternRecChannelScanner::ExternRecChannelScanner(uint cardid,
                                                 QString inputname,
                                                 uint sourceid,
                                                 ScanMonitor *monitor)
    : m_scan_monitor(monitor)
    , m_cardid(cardid)
    , m_inputname(std::move(inputname))
    , m_sourceid(sourceid)
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

    while (m_thread_running)
    {
        m_stop_now = true;
        m_lock.unlock();
        m_thread->wait(5);
        m_lock.lock();
    }

    m_lock.unlock();

    m_thread->wait();
}

/// \brief Scans the list.
void ExternRecChannelScanner::Scan(void)
{
    Stop();
    m_stop_now = false;
    m_thread->start();
}

void ExternRecChannelScanner::run(void)
{
    m_lock.lock();
    m_thread_running = true;
    m_lock.unlock();


    // Step 1/4 : Get info from DB
    QString cmd = CardUtil::GetVideoDevice(m_cardid);

    if (m_stop_now || cmd.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Invalid external command");
        QMutexLocker locker(&m_lock);
        m_thread_running = false;
        m_stop_now = true;
        return;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("External Command: '%1'").arg(cmd));


    // Step 2/4 : Download
    if (m_scan_monitor)
    {
        m_scan_monitor->ScanPercentComplete(1);
        m_scan_monitor->ScanAppendTextToLog(tr("Creating channel list"));
    }

    ExternalRecChannelFetcher fetch(m_cardid, cmd);

    if ((m_channel_total = fetch.LoadChannels()) < 1)
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "Failed to load channels");
        QMutexLocker locker(&m_lock);
        m_thread_running = false;
        m_stop_now = true;
        return;
    }

    vector<uint> existing = ChannelUtil::GetChanIDs(m_sourceid);
    vector<uint>::iterator Iold;

    // Step 3/4 : Process
    if (m_scan_monitor)
        m_scan_monitor->ScanAppendTextToLog(tr("Processing channels"));

    QString channum;
    QString name;
    QString callsign;
    QString xmltvid;
    int     cnt = 0;

    if (!fetch.FirstChannel(channum, name, callsign, xmltvid))
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + "No channels found.");
        QMutexLocker locker(&m_lock);
        m_thread_running = false;
        m_stop_now = true;
        return;
    }

    if (m_scan_monitor)
        m_scan_monitor->ScanAppendTextToLog(tr("Adding Channels"));

    uint idx = 0;
    for (;;)
    {
        QString msg = tr("Channel #%1 : %2").arg(channum).arg(name);

        LOG(VB_CHANNEL, LOG_INFO, QString("Handling channel %1 %2")
            .arg(channum).arg(name));

        int chanid = ChannelUtil::GetChanID(m_sourceid, channum);

        if (m_scan_monitor)
            m_scan_monitor->ScanPercentComplete(++cnt * 100 / m_channel_total);

        if (chanid <= 0)
        {
            if (m_scan_monitor)
                m_scan_monitor->ScanAppendTextToLog(tr("Adding %1").arg(msg));

            chanid = ChannelUtil::CreateChanID(m_sourceid, channum);
            ChannelUtil::CreateChannel(0, m_sourceid, chanid, callsign, name,
                                       channum, 1, 0, 0,
                                       false, false, false, QString(),
                                       QString(), "Default", xmltvid);
        }
        else
        {
            if (m_scan_monitor)
                m_scan_monitor->ScanAppendTextToLog(tr("Updating %1").arg(msg));

            ChannelUtil::UpdateChannel(0, m_sourceid, chanid, callsign, name,
                                       channum, 1, 0, 0,
                                       false, false, false, QString(),
                                       QString(), "Default", xmltvid);
        }

        SetNumChannelsInserted(cnt);

        if ((Iold = std::find(existing.begin(), existing.end(), chanid)) !=
             existing.end())
        {
            existing.erase(Iold);
        }

        if (++idx < m_channel_total)
            fetch.NextChannel(channum, name, callsign, xmltvid);
        else
            break;
    }

    // Remove any channels which are no longer valid
    for (Iold = existing.begin(); Iold != existing.end(); ++Iold)
    {
        channum = ChannelUtil::GetChanNum(*Iold);

        if (m_scan_monitor)
            m_scan_monitor->ScanAppendTextToLog
                (tr("Removing unused Channel #%1").arg(channum));

        ChannelUtil::DeleteChannel(*Iold);
    }

    // Step 4/4 : Finish up
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Found %1 channels").arg(cnt));
    if (m_scan_monitor)
        m_scan_monitor->ScanAppendTextToLog
            (tr("Found %1 channels.").arg(cnt));

    if (m_scan_monitor)
    {
        m_scan_monitor->ScanAppendTextToLog(tr("Done"));
        m_scan_monitor->ScanAppendTextToLog("");
        m_scan_monitor->ScanPercentComplete(100);
        m_scan_monitor->ScanComplete();
    }

    QMutexLocker locker(&m_lock);
    m_thread_running = false;
    m_stop_now = true;
}

void ExternRecChannelScanner::SetNumChannelsParsed(uint val)
{
    uint minval = 35;
    uint range = 70 - minval;
    uint pct = minval + (uint) truncf((((float)val) / m_channel_cnt) * range);
    if (m_scan_monitor)
        m_scan_monitor->ScanPercentComplete(pct);
}

void ExternRecChannelScanner::SetNumChannelsInserted(uint val)
{
    uint minval = 70;
    uint range = 100 - minval;
    uint pct = minval + (uint) truncf((((float)val) / m_channel_cnt) * range);
    if (m_scan_monitor)
        m_scan_monitor->ScanPercentComplete(pct);
}

void ExternRecChannelScanner::SetMessage(const QString &status)
{
    if (m_scan_monitor)
        m_scan_monitor->ScanAppendTextToLog(status);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
