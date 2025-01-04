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
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"

#include "cardutil.h"
#include "channelutil.h"
#include "iptvchannelfetcher.h"
#include "scanmonitor.h"

#define LOC QString("IPTVChanFetch: ")

static bool parse_chan_info(const QString   &rawdata,
                            IPTVChannelInfo &info,
                            QString         &channum,
                            uint            &lineNum);

static bool parse_extinf(const QString &line,
                         QString       &channum,
                         QString       &name,
                         QString       &logo);

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

// Download the IPTV channel logo and write to configured location.
static bool download_logo(const QString& logoUrl, const QString& filename)
{
    bool ret = false;

    if (!logoUrl.isEmpty())
    {
        QByteArray data;
        if (GetMythDownloadManager()->download(logoUrl, &data))
        {
            QString iconDir = GetConfDir() + "/channels/";
            QString filepath = iconDir + filename;
            QFile file(filepath);
            if (file.open(QIODevice::WriteOnly))
            {
                if (file.write(data) > 0)
                {
                    ret = true;
                    LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                        QString("DownloadLogo to file %1").arg(filepath));
                }
                else
                {
                    LOG(VB_CHANNEL, LOG_ERR, LOC +
                        QString("DownloadLogo failed to write to file %1").arg(filepath));
                }
                file.close();
            }
            else
            {
                LOG(VB_CHANNEL, LOG_ERR, LOC +
                    QString("DownloadLogo failed to open file %1").arg(filepath));
            }
        }
        else
        {
            LOG(VB_CHANNEL, LOG_ERR, LOC +
                QString("DownloadLogo failed to download %1").arg(logoUrl));
        }
    }
    else
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + "DownloadLogo empty logoUrl");
    }
    return ret;
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
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Playlist URL is empty");
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Playlist URL: %1").arg(url));

    // Step 2/4 : Download
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanAppendTextToLog(tr("Downloading Playlist"));
        m_scanMonitor->ScanPercentComplete(5);
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

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Playlist file size: %1").arg(playlist.length()));

    // Step 3/4 : Process
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanAppendTextToLog(tr("Processing Playlist"));
        m_scanMonitor->ScanPercentComplete(35);
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
        // Sort channels in ascending channel number order
        std::vector<fbox_chan_map_t::const_iterator> acno (m_channels.size());
        {
            fbox_chan_map_t::const_iterator it = m_channels.cbegin();
            for (uint i = 0; it != m_channels.cend(); ++it, ++i)
            {
                acno[i] = it;
            }
            std::sort(acno.begin(), acno.end(),
                [] (fbox_chan_map_t::const_iterator s1, fbox_chan_map_t::const_iterator s2) -> bool
                {
                    return s1.key().toInt() < s2.key().toInt();
                }
            );
        }

        // Insert channels in ascending channel number order
        for (uint i = 0; i < acno.size(); ++i)
        {
            fbox_chan_map_t::const_iterator it = acno[i];
            const QString& channum = it.key();
            QString name    = (*it).m_name;
            QString xmltvid = (*it).m_xmltvid.isEmpty() ? "" : (*it).m_xmltvid;
            QString logoUrl = (*it).m_logo;
            QString logo;

            // Download channel icon (logo) when there is an logo URL in the EXTINF
            if (!logoUrl.isEmpty())
            {
                QString filename = QString("IPTV_%1_%2_logo").arg(m_sourceId).arg(channum);
                if (download_logo(logoUrl, filename))
                {
                    logo = filename;
                }
            }

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
                                           logo, "Default", xmltvid);
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
                                           logo, "Default", xmltvid);
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
        QString ret;
        QUrl qurl(url);
        QFile file(qurl.toLocalFile());
        if (!file.open(QIODevice::ReadOnly))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Opening '%1'")
                    .arg(qurl.toLocalFile()) + ENO);
            return ret;
        }

        while (!file.atEnd())
        {
            QByteArray data = file.readLine();
            ret += QString(data);
        }

        file.close();
        return ret;
    }

    // Use MythDownloadManager for http URLs
    QByteArray data;
    QString ret;

    if (GetMythDownloadManager()->download(url, &data))
    {
        ret = QString(data);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("DownloadPlaylist failed to "
                    "download from %1").arg(url));
    }
    return ret;
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

    // Estimate number of channels
    if (fetcher)
    {
        uint num_channels = estimate_number_of_channels(rawdata);
        fetcher->SetTotalNumChannels(num_channels);

        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Estimating there are %1 channels in playlist")
                .arg(num_channels));
    }

    // Get the next available channel number for the source (only used if we can't find one in the playlist)
    if (fetcher)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        QString sql = "SELECT MAX(CONVERT(channum, UNSIGNED INTEGER)) FROM channel "
                      "WHERE sourceid = :SOURCEID  AND  deleted IS NULL";

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
                LOG(VB_CHANNEL, LOG_INFO, LOC +
                    QString("Next available channel number from DB is: %1").arg(nextChanNum));
            }
            else
            {
                nextChanNum = 1;
                LOG(VB_CHANNEL, LOG_INFO, LOC +
                    QString("No channels found for this source, using default channel number: %1").arg(nextChanNum));
            }
        }
    }

    // Parse each channel
    uint lineNum = 1;
    for (uint i = 1; true; ++i)
    {
        IPTVChannelInfo info;
        QString channum;

        if (!parse_chan_info(rawdata, info, channum, lineNum))
            break;

        // Check if parsed channel number is valid
        if (!channum.isEmpty())
        {
            bool ok = false;
            int channel_number = channum.toInt(&ok);
            if (ok && channel_number > 0)
            {
                // Positive integer channel number found
            }
            else
            {
                // Ignore invalid or negative channel numbers
                channum.clear();
            }
        }

        // No channel number found, try to find channel in database and use that channel number
        if (fetcher)
        {
            if (channum.isEmpty())
            {
                uint sourceId = fetcher->m_sourceId;
                QString channum_db = ChannelUtil::GetChannelNumber(sourceId, info.m_name);
                if (!channum_db.isEmpty())
                {
                    LOG(VB_RECORD, LOG_INFO, LOC +
                        QString("Found channel in database, channel number: %1 for channel: %2")
                            .arg(channum_db, info.m_name));

                    channum = channum_db;
                }
            }
        }

        // Update highest channel number found so far
        if (!channum.isEmpty())
        {
            bool ok = false;
            int channel_number = channum.toInt(&ok);
            if (ok && channel_number > 0)
            {
                if (channel_number >= nextChanNum)
                {
                    nextChanNum = channel_number + 1;
                }
            }
        }

        // No channel number found, use the default next one
        if (channum.isEmpty())
        {
            LOG(VB_CHANNEL, LOG_INFO, QString("No channel number found, using next available: %1 for channel: %2")
                .arg(nextChanNum).arg(info.m_name));
            channum = QString::number(nextChanNum);
            nextChanNum++;
        }

        QString msg = tr("Encountered malformed channel");
        if (!channum.isEmpty())
        {
            chanmap[channum] = info;

            msg = QString("Parsing Channel #%1: '%2' %3")
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
    QString logo;
    QMap<QString,QString> values;

    while (true)
    {
        QString line = rawdata.section("\n", lineNum, lineNum, QString::SectionSkipEmpty);
        if (line.isEmpty())
            return false;

        LOG(VB_CHANNEL, LOG_INFO, LOC + line);

        ++lineNum;
        if (line.startsWith("#"))
        {
            if (line.startsWith("#EXTINF:"))
            {
                parse_extinf(line.mid(line.indexOf(':')+1), channum, name, logo);
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

        LOG(VB_CHANNEL, LOG_DEBUG, LOC +
            QString("parse_chan_info name='%2'").arg(name));
        LOG(VB_CHANNEL, LOG_DEBUG, LOC +
            QString("parse_chan_info channum='%2'").arg(channum));
        LOG(VB_CHANNEL, LOG_DEBUG, LOC +
            QString("parse_chan_info logo='%2'").arg(logo));
        for (auto it = values.cbegin(); it != values.cend(); ++it)
        {
            LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                QString("parse_chan_info [%1]='%2'")
                .arg(it.key(), *it));
        }
        info = IPTVChannelInfo(
            name, values["xmltvid"], logo,
            line, values["bitrate"].toUInt(),
            values["fectype"],
            values["fecurl0"], values["fecbitrate0"].toUInt(),
            values["fecurl1"], values["fecbitrate1"].toUInt(),
            values["programnumber"].toUInt());
        return true;
    }
}

// Search for channel name in last part of the EXTINF line
static QString parse_extinf_name_trailing(const QString& line)
{
    QString result;
    static const QRegularExpression re_name { R"([^\,+],(.*)$)" };
    auto match = re_name.match(line);
    if (match.hasMatch())
    {
        result = match.captured(1).simplified();
    }
    return result;
}

// Search for field value, e.g. field="value", in EXTINF line
static QString parse_extinf_field(QString line, const QString& field)
{
    QString result;
    auto pos = line.indexOf(field, 0, Qt::CaseInsensitive);
    if (pos > 0)
    {
        line.remove(0, pos);
        static const QRegularExpression re { R"(\"([^\"]*)\"(.*)$)" };
        auto match = re.match(line);
        if (match.hasMatch())
        {
            result = match.captured(1).simplified();
        }
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("line:%1  field:%2  result:%3")
        .arg(line, field, result));

    return result;
}

// Search for channel number, channel name and channel logo in EXTINF line
static bool parse_extinf(const QString &line,
                         QString       &channum,
                         QString       &name,
                         QString       &logo)
{

    // Parse EXTINF line with TVG fields, Zatto style
    // EG. #EXTINF:0001 tvg-id="ITV1London.uk" tvg-chno="90001" group-title="General Interest" tvg-logo="https://images.zattic.com/logos/ee3c9d2ac083eb2154b5/black/210x120.png", ITV 1 HD

    // Parse EXTINF line, HDHomeRun style
    // EG. #EXTINF:-1 channel-id="22" channel-number="22" tvg-name="Omroep Brabant",22 Omroep Brabant
    //     #EXTINF:-1 channel-id="2.1" channel-number="2.1" tvg-name="CBS2-HD",2.1 CBS2-HD

    // Parse EXTINF line, https://github.com/iptv-org/iptv/blob/master/channels/ style
    // EG. #EXTINF:-1 tvg-id="" tvg-name="" tvg-logo="https://i.imgur.com/VejnhiB.png" group-title="News",BBC News
    {
        channum = parse_extinf_field(line, "tvg-chno");
        if (channum.isEmpty())
        {
            channum = parse_extinf_field(line, "channel-number");
        }
        logo = parse_extinf_field(line, "tvg-logo");
        name = parse_extinf_field(line, "tvg-name");
        if (name.isEmpty())
        {
            name = parse_extinf_name_trailing(line);
        }

        // If we only have the name then it might be another format
        if (!name.isEmpty() && !channum.isEmpty())
        {
            return true;
        }
    }

    // Freebox or SAT>IP format
    {
        static const QRegularExpression re
            { R"(^-?\d+,(\d+)(?:\.\s|\s-\s)(.*)$)" };
        auto match = re.match(line);
        if (match.hasMatch())
        {
            channum = match.captured(1);
            name = match.captured(2);
            if (!channum.isEmpty() && !name.isEmpty())
            {
                return true;
            }
        }
    }

    // Moviestar TV number then name
    {
        static const QRegularExpression re
            { R"(^-?\d+,\[(\d+)\]\s+(.*)$)" };
        auto match = re.match(line);
        if (match.hasMatch())
        {
            channum = match.captured(1);
            name = match.captured(2);
            if (!channum.isEmpty() && !name.isEmpty())
            {
                return true;
            }
        }
    }

    // Moviestar TV name then number
    {
        static const QRegularExpression re
            { R"(^-?\d+,(.*)\s+\[(\d+)\]$)" };
        auto match = re.match(line);
        if (match.hasMatch())
        {
            channum = match.captured(2);
            name = match.captured(1);
            if (!channum.isEmpty() && !name.isEmpty())
            {
                return true;
            }
        }
    }

    // Parse russion iptv plugin style
    {
        static const QRegularExpression re
            { R"(^(-?\d+)\s+[^,]*,\s*(.*)$)" };
        auto match = re.match(line);
        if (match.hasMatch())
        {
            channum = match.captured(1).simplified();
            name = match.captured(2).simplified();
            bool ok = false;
            int channel_number = channum.toInt (&ok);
            if (ok && (channel_number > 0) && !name.isEmpty())
            {
                return true;
            }
        }
    }

    // Almost a catchall: just get the name from the end of the line
    // EG. #EXTINF:-1,Channel Title
    name = parse_extinf_name_trailing(line);
    if (!name.isEmpty())
    {
        return true;
    }

    // Not one of the formats we support
    QString msg = LOC + QString("Invalid header in channel list line \n\t\t\tEXTINF:%1").arg(line);
    LOG(VB_GENERAL, LOG_ERR, msg);
    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
