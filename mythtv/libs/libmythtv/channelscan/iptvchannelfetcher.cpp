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
#include "iptvchannelfetcher.h"
#include "scanmonitor.h"
#include "mythlogging.h"
#include "mythdownloadmanager.h"

#define LOC QString("IPTVChanFetch: ")

static bool parse_chan_info(const QString   &rawdata,
                            IPTVChannelInfo &info,
                            QString         &channum,
                            int             &nextChanNum,
                            uint            &lineNum);

static bool parse_extinf(const QString &line,
                         QString       &channum,
                         QString       &name,
                         int           &nextChanNum);

IPTVChannelFetcher::IPTVChannelFetcher(
    uint cardid, QString inputname, uint sourceid,
    bool is_mpts, ScanMonitor *monitor) :
    m_scanMonitor(monitor),
    m_cardId(cardid),       m_inputName(std::move(inputname)),
    m_sourceId(sourceid),   m_isMpts(is_mpts),
    m_thread(new MThread("IPTVChannelFetcher", this))
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Has ScanMonitor %1")
        .arg(monitor?"true":"false"));
}

IPTVChannelFetcher::~IPTVChannelFetcher()
{
    Stop();
    delete m_thread;
    m_thread = nullptr;
}

/** \fn IPTVChannelFetcher::Stop(void)
 *  \brief Stops the scanning thread running
 */
void IPTVChannelFetcher::Stop(void)
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

fbox_chan_map_t IPTVChannelFetcher::GetChannels(void)
{
    while (!m_thread->isFinished())
        m_thread->wait(500ms);

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Found %1 channels")
        .arg(m_channels.size()));
    return m_channels;
}

/// \brief Scans the given frequency list.
void IPTVChannelFetcher::Scan(void)
{
    Stop();
    m_stopNow = false;
    m_thread->start();
}

void IPTVChannelFetcher::run(void)
{
    m_lock.lock();
    m_threadRunning = true;
    m_lock.unlock();

    // Step 1/4 : Get info from DB
    QString url = CardUtil::GetVideoDevice(m_cardId);

    if (m_stopNow || url.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Playlist URL was empty");
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Playlist URL: %1").arg(url));

    // Step 2/4 : Download
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanPercentComplete(5);
        m_scanMonitor->ScanAppendTextToLog(tr("Downloading Playlist"));
    }

    QString playlist = DownloadPlaylist(url);

    if (m_stopNow || playlist.isEmpty())
    {
        if (playlist.isNull() && m_scanMonitor)
        {
            m_scanMonitor->ScanAppendTextToLog(
                QCoreApplication::translate("(Common)", "Error"));
            m_scanMonitor->ScanPercentComplete(100);
            m_scanMonitor->ScanErrored(tr("Downloading Playlist Failed"));
        }
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    // Step 3/4 : Process
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanPercentComplete(35);
        m_scanMonitor->ScanAppendTextToLog(tr("Processing Playlist"));
    }

    m_channels.clear();
    m_channels = ParsePlaylist(playlist, this);

    // Step 4/4 : Finish up
    if (m_scanMonitor)
        m_scanMonitor->ScanAppendTextToLog(tr("Adding Channels"));
    SetTotalNumChannels(m_channels.size());

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Found %1 channels")
        .arg(m_channels.size()));

    if (!m_isMpts)
    {
        fbox_chan_map_t::const_iterator it = m_channels.cbegin();
        for (uint i = 1; it != m_channels.cend(); ++it, ++i)
        {
            const QString& channum = it.key();
            QString name    = (*it).m_name;
            QString xmltvid = (*it).m_xmltvid.isEmpty() ? "" : (*it).m_xmltvid;
            uint programnumber = (*it).m_programNumber;
            //: %1 is the channel number, %2 is the channel name
            QString msg = tr("Channel #%1 : %2").arg(channum, name);

            LOG(VB_CHANNEL, LOG_INFO, QString("Handling channel %1 %2")
                .arg(channum, name));

            int chanid = ChannelUtil::GetChanID(m_sourceId, channum);
            if (chanid <= 0)
            {
                if (m_scanMonitor)
                {
                    m_scanMonitor->ScanAppendTextToLog(tr("Adding %1").arg(msg));
                }
                chanid = ChannelUtil::CreateChanID(m_sourceId, channum);
                ChannelUtil::CreateChannel(0, m_sourceId, chanid, name, name,
                                           channum, programnumber, 0, 0,
                                           false, kChannelVisible, QString(),
                                           QString(), "Default", xmltvid);
                ChannelUtil::CreateIPTVTuningData(chanid, (*it).m_tuning);
            }
            else
            {
                if (m_scanMonitor)
                {
                    m_scanMonitor->ScanAppendTextToLog(
                                               tr("Updating %1").arg(msg));
                }
                ChannelUtil::UpdateChannel(0, m_sourceId, chanid, name, name,
                                           channum, programnumber, 0, 0,
                                           false, kChannelVisible, QString(),
                                           QString(), "Default", xmltvid);
                ChannelUtil::UpdateIPTVTuningData(chanid, (*it).m_tuning);
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
    }

    QMutexLocker locker(&m_lock);
    m_threadRunning = false;
    m_stopNow = true;
}

void IPTVChannelFetcher::SetNumChannelsParsed(uint val)
{
    uint minval = 35;
    uint range = 70 - minval;
    uint pct = minval + (uint) truncf((((float)val) / m_chanCnt) * range);
    if (m_scanMonitor)
        m_scanMonitor->ScanPercentComplete(pct);
}

void IPTVChannelFetcher::SetNumChannelsInserted(uint val)
{
    uint minval = 70;
    uint range = 100 - minval;
    uint pct = minval + (uint) truncf((((float)val) / m_chanCnt) * range);
    if (m_scanMonitor)
        m_scanMonitor->ScanPercentComplete(pct);
}

void IPTVChannelFetcher::SetMessage(const QString &status)
{
    if (m_scanMonitor)
        m_scanMonitor->ScanAppendTextToLog(status);
}

// This function is always called from a thread context.
QString IPTVChannelFetcher::DownloadPlaylist(const QString &url)
{
    if (url.startsWith("file", Qt::CaseInsensitive))
    {
        QString ret = "";
        QUrl qurl(url);
        QFile file(qurl.toLocalFile());
        if (!file.open(QIODevice::ReadOnly))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Opening '%1'")
                    .arg(qurl.toLocalFile()) + ENO);
            return ret;
        }

        QTextStream stream(&file);
        while (!stream.atEnd())
            ret += stream.readLine() + "\n";

        file.close();
        return ret;
    }

    // Use MythDownloadManager for http URLs
    QByteArray data;
    QString tmp;

    if (!GetMythDownloadManager()->download(url, &data))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("DownloadPlaylist failed to "
                    "download from %1").arg(url));
    }
    else
        tmp = QString(data);

    return tmp.isNull() ? tmp : QString::fromUtf8(tmp.toLatin1().constData());
}

static uint estimate_number_of_channels(const QString &rawdata)
{
    uint result = 0;
    uint numLine = 1;
    while (true)
    {
        QString url = rawdata.section("\n", numLine, numLine, QString::SectionSkipEmpty);
        if (url.isEmpty())
            return result;

        ++numLine;
        if (!url.startsWith("#")) // ignore comments
            ++result;
    }
}

fbox_chan_map_t IPTVChannelFetcher::ParsePlaylist(
    const QString &reallyrawdata, IPTVChannelFetcher *fetcher)
{
    fbox_chan_map_t chanmap;
    int nextChanNum = 1;

    QString rawdata = reallyrawdata;
    rawdata.replace("\r\n", "\n");

    // Verify header is ok
    QString header = rawdata.section("\n", 0, 0);
    if (!header.startsWith("#EXTM3U"))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Invalid channel list header (%1)").arg(header));

        if (fetcher)
        {
            fetcher->SetMessage(
                tr("ERROR: M3U channel list is malformed"));
        }

        return chanmap;
    }

    // estimate number of channels
    if (fetcher)
    {
        uint num_channels = estimate_number_of_channels(rawdata);
        fetcher->SetTotalNumChannels(num_channels);

        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Estimating there are %1 channels in playlist")
                .arg(num_channels));
    }

    // get the next available channel number for the source (only used if we can't find one in the playlist)
    if (fetcher)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        QString sql = "select MAX(CONVERT(channum, UNSIGNED INTEGER)) from channel where sourceid = :SOURCEID;";

        query.prepare(sql);
        query.bindValue(":SOURCEID", fetcher->m_sourceId);

        if (!query.exec())
        {
            MythDB::DBError("Get next max channel number", query);
        }
        else
        {
            if (query.first())
            {
                nextChanNum = query.value(0).toInt() + 1;
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Next available channel number from DB is: %1").arg(nextChanNum));
            }
            else
            {
                nextChanNum = 1;
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("No channels found for this source, using default channel number: %1").arg(nextChanNum));
            }
        }
    }

    // Parse each channel
    uint lineNum = 1;
    for (uint i = 1; true; ++i)
    {
        IPTVChannelInfo info;
        QString channum;

        if (!parse_chan_info(rawdata, info, channum, nextChanNum, lineNum))
            break;

        QString msg = tr("Encountered malformed channel");
        if (!channum.isEmpty())
        {
            chanmap[channum] = info;

            msg = QString("Parsing Channel #%1 : %2 : %3")
                .arg(channum, info.m_name,
                     info.m_tuning.GetDataURL().toString());
            LOG(VB_CHANNEL, LOG_INFO, LOC + msg);

            msg.clear(); // don't tell fetcher
        }

        if (fetcher)
        {
            if (!msg.isEmpty())
                fetcher->SetMessage(msg);
            fetcher->SetNumChannelsParsed(i);
        }
    }

    return chanmap;
}

static bool parse_chan_info(const QString   &rawdata,
                            IPTVChannelInfo &info,
                            QString         &channum,
                            int             &nextChanNum,
                            uint            &lineNum)
{
    // #EXTINF:0,2 - France 2                <-- duration,channum - channame
    // #EXTMYTHTV:xmltvid=C2.telepoche.com   <-- optional line (myth specific)
    // #EXTMYTHTV:bitrate=BITRATE            <-- optional line (myth specific)
    // #EXTMYTHTV:fectype=FECTYPE            <-- optional line (myth specific)
    //     The FECTYPE can be rfc2733, rfc5109, or smpte2022
    // #EXTMYTHTV:fecurl0=URL                <-- optional line (myth specific)
    // #EXTMYTHTV:fecurl1=URL                <-- optional line (myth specific)
    // #EXTMYTHTV:fecbitrate0=BITRATE        <-- optional line (myth specific)
    // #EXTMYTHTV:fecbitrate1=BITRATE        <-- optional line (myth specific)
    // #EXTVLCOPT:program=program_number     <-- optional line (used by MythTV and VLC)
    // #...                                  <-- ignored comments
    // rtsp://maiptv.iptv.fr/iptvtv/201 <-- url

    QString name;
    QMap<QString,QString> values;

    while (true)
    {
        QString line = rawdata.section("\n", lineNum, lineNum, QString::SectionSkipEmpty);
        if (line.isEmpty())
            return false;

        ++lineNum;
        if (line.startsWith("#"))
        {
            if (line.startsWith("#EXTINF:"))
            {
                parse_extinf(line.mid(line.indexOf(':')+1), channum, name, nextChanNum);
            }
            else if (line.startsWith("#EXTMYTHTV:"))
            {
                QString data = line.mid(line.indexOf(':')+1);
                QString key = data.left(data.indexOf('='));
                if (!key.isEmpty())
                {
                    if (key == "name")
                        name = data.mid(data.indexOf('=')+1);
                    else if (key == "channum")
                        channum = data.mid(data.indexOf('=')+1);
                    else
                        values[key] = data.mid(data.indexOf('=')+1);
                }
            }
            else if (line.startsWith("#EXTVLCOPT:program="))
            {
                values["programnumber"] = line.mid(line.indexOf('=')+1);
            }
            continue;
        }

        if (name.isEmpty())
            return false;

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("parse_chan_info name='%2'").arg(name));
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("parse_chan_info channum='%2'").arg(channum));
        for (auto it = values.cbegin(); it != values.cend(); ++it)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("parse_chan_info [%1]='%2'")
                .arg(it.key(), *it));
        }
        info = IPTVChannelInfo(
            name, values["xmltvid"],
            line, values["bitrate"].toUInt(),
            values["fectype"],
            values["fecurl0"], values["fecbitrate0"].toUInt(),
            values["fecurl1"], values["fecbitrate1"].toUInt(),
            values["programnumber"].toUInt());
        return true;
    }
}

static bool parse_extinf(const QString &line,
                         QString       &channum,
                         QString       &name,
                         int           &nextChanNum)
{
    // Parse extension portion, Freebox or SAT>IP format
    static const QRegularExpression chanNumName1
        { R"(^-?\d+,(\d+)(?:\.\s|\s-\s)(.*)$)" };
    auto match = chanNumName1.match(line);
    if (match.hasMatch())
    {
        channum = match.captured(1);
        name = match.captured(2);
        return true;
    }

    // Parse extension portion, A1 TV format
    static const QRegularExpression chanNumName2
        { "^-?\\d+\\s+[^,]*tvg-num=\"(\\d+)\"[^,]*,(.*)$" };
    match = chanNumName2.match(line);
    if (match.hasMatch())
    {
        channum = match.captured(1);
        name = match.captured(2);
        return true;
    }

    // Parse extension portion, Moviestar TV number then name
    static const QRegularExpression chanNumName3
        { R"(^-?\d+,\[(\d+)\]\s+(.*)$)" };
    match = chanNumName3.match(line);
    if (match.hasMatch())
    {
        channum = match.captured(1);
        name = match.captured(2);
        return true;
    }

    // Parse extension portion, Moviestar TV name then number
    static const QRegularExpression chanNumName4
        { R"(^-?\d+,(.*)\s+\[(\d+)\]$)" };
    match = chanNumName4.match(line);
    if (match.hasMatch())
    {
        channum = match.captured(2);
        name = match.captured(1);
        return true;
    }

    // Parse extension portion, russion iptv plugin style
    static const QRegularExpression chanNumName5
        { R"(^(-?\d+)\s+[^,]*,\s*(.*)$)" };
    match = chanNumName5.match(line);
    if (match.hasMatch())
    {
        channum = match.captured(1).simplified();
        name = match.captured(2).simplified();
        bool ok = false;
        int channel_number = channum.toInt (&ok);
        if (ok && (channel_number > 0))
        {
            return true;
        }
    }

    // Parse extension portion, https://github.com/iptv-org/iptv/blob/master/channels/ style
    // EG. #EXTINF:-1 tvg-id="" tvg-name="" tvg-logo="https://i.imgur.com/VejnhiB.png" group-title="News",BBC News
    static const QRegularExpression chanNumName6
        { "(^-?\\d+)\\s+[^,]*[^,]*,(.*)$" };
    match = chanNumName6.match(line);
    if (match.hasMatch())
    {
        channum = match.captured(1).simplified();
        name = match.captured(2).simplified();

        bool ok = false;
        int channel_number = channum.toInt(&ok);
        if (ok && channel_number > 0)
        {
            if (channel_number >= nextChanNum)
                nextChanNum = channel_number + 1;
            return true;
        }

        // no valid channel number found use the default next one
        LOG(VB_GENERAL, LOG_ERR, QString("No channel number found, using next available: %1 for channel: %2").arg(nextChanNum).arg(name));
        channum = QString::number(nextChanNum);
        nextChanNum++;
        return true;
    }

    // not one of the formats we support
    QString msg = LOC + QString("Invalid header in channel list line \n\t\t\tEXTINF:%1").arg(line);
    LOG(VB_GENERAL, LOG_ERR, msg);
    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
