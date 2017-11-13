// -*- Mode: c++ -*-

// Std C headers
#include <cmath>
#include <unistd.h>

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
                            uint            &lineNum);

static bool parse_extinf(const QString &data,
                         QString       &channum,
                         QString       &name);

IPTVChannelFetcher::IPTVChannelFetcher(
    uint cardid, const QString &inputname, uint sourceid,
    bool is_mpts, ScanMonitor *monitor) :
    _scan_monitor(monitor),
    _cardid(cardid),       _inputname(inputname),
    _sourceid(sourceid),   _is_mpts(is_mpts),
    _chan_cnt(1),          _thread_running(false),
    _stop_now(false),      _thread(new MThread("IPTVChannelFetcher", this)),
    _lock()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Has ScanMonitor %1")
        .arg(monitor?"true":"false"));
}

IPTVChannelFetcher::~IPTVChannelFetcher()
{
    Stop();
    delete _thread;
    _thread = NULL;
}

/** \fn IPTVChannelFetcher::Stop(void)
 *  \brief Stops the scanning thread running
 */
void IPTVChannelFetcher::Stop(void)
{
    _lock.lock();

    while (_thread_running)
    {
        _stop_now = true;
        _lock.unlock();
        _thread->wait(5);
        _lock.lock();
    }

    _lock.unlock();

    _thread->wait();
}

fbox_chan_map_t IPTVChannelFetcher::GetChannels(void)
{
    while (!_thread->isFinished())
        _thread->wait(500);

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Found %1 channels")
        .arg(_channels.size()));
    return _channels;
}

/// \brief Scans the given frequency list.
void IPTVChannelFetcher::Scan(void)
{
    Stop();
    _stop_now = false;
    _thread->start();
}

void IPTVChannelFetcher::run(void)
{
    _lock.lock();
    _thread_running = true;
    _lock.unlock();

    // Step 1/4 : Get info from DB
    QString url = CardUtil::GetVideoDevice(_cardid);

    if (_stop_now || url.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Playlist URL was empty");
        QMutexLocker locker(&_lock);
        _thread_running = false;
        _stop_now = true;
        return;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Playlist URL: %1").arg(url));

    // Step 2/4 : Download
    if (_scan_monitor)
    {
        _scan_monitor->ScanPercentComplete(5);
        _scan_monitor->ScanAppendTextToLog(tr("Downloading Playlist"));
    }

    QString playlist = DownloadPlaylist(url);

    if (_stop_now || playlist.isEmpty())
    {
        if (playlist.isNull() && _scan_monitor)
        {
            _scan_monitor->ScanAppendTextToLog(
                QCoreApplication::translate("(Common)", "Error"));
            _scan_monitor->ScanPercentComplete(100);
            _scan_monitor->ScanErrored(tr("Downloading Playlist Failed"));
        }
        QMutexLocker locker(&_lock);
        _thread_running = false;
        _stop_now = true;
        return;
    }

    // Step 3/4 : Process
    if (_scan_monitor)
    {
        _scan_monitor->ScanPercentComplete(35);
        _scan_monitor->ScanAppendTextToLog(tr("Processing Playlist"));
    }

    _channels.clear();
    _channels = ParsePlaylist(playlist, this);

    // Step 4/4 : Finish up
    if (_scan_monitor)
        _scan_monitor->ScanAppendTextToLog(tr("Adding Channels"));
    SetTotalNumChannels(_channels.size());

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Found %1 channels")
        .arg(_channels.size()));

    if (!_is_mpts)
    {
        fbox_chan_map_t::const_iterator it = _channels.begin();
        for (uint i = 1; it != _channels.end(); ++it, ++i)
        {
            QString channum = it.key();
            QString name    = (*it).m_name;
            QString xmltvid = (*it).m_xmltvid.isEmpty() ? "" : (*it).m_xmltvid;
            uint programnumber = (*it).m_programNumber;
            //: %1 is the channel number, %2 is the channel name
            QString msg = tr("Channel #%1 : %2").arg(channum).arg(name);

            LOG(VB_CHANNEL, LOG_INFO, QString("Handling channel %1 %2")
                .arg(channum).arg(name));

            int chanid = ChannelUtil::GetChanID(_sourceid, channum);
            if (chanid <= 0)
            {
                if (_scan_monitor)
                {
                    _scan_monitor->ScanAppendTextToLog(
                                                       tr("Adding %1").arg(msg));
                }
                chanid = ChannelUtil::CreateChanID(_sourceid, channum);
                ChannelUtil::CreateChannel(0, _sourceid, chanid, name, name,
                                           channum, programnumber, 0, 0,
                                           false, false, false, QString::null,
                                           QString::null, "Default", xmltvid);
                ChannelUtil::CreateIPTVTuningData(chanid, (*it).m_tuning);
            }
            else
            {
                if (_scan_monitor)
                {
                    _scan_monitor->ScanAppendTextToLog(
                                               tr("Updating %1").arg(msg));
                }
                ChannelUtil::UpdateChannel(0, _sourceid, chanid, name, name,
                                           channum, programnumber, 0, 0,
                                           false, false, false, QString::null,
                                           QString::null, "Default", xmltvid);
                ChannelUtil::UpdateIPTVTuningData(chanid, (*it).m_tuning);
            }

            SetNumChannelsInserted(i);
        }

        if (_scan_monitor)
        {
            _scan_monitor->ScanAppendTextToLog(tr("Done"));
            _scan_monitor->ScanAppendTextToLog("");
            _scan_monitor->ScanPercentComplete(100);
            _scan_monitor->ScanComplete();
        }
    }

    QMutexLocker locker(&_lock);
    _thread_running = false;
    _stop_now = true;
}

void IPTVChannelFetcher::SetNumChannelsParsed(uint val)
{
    uint minval = 35, range = 70 - minval;
    uint pct = minval + (uint) truncf((((float)val) / _chan_cnt) * range);
    if (_scan_monitor)
        _scan_monitor->ScanPercentComplete(pct);
}

void IPTVChannelFetcher::SetNumChannelsInserted(uint val)
{
    uint minval = 70, range = 100 - minval;
    uint pct = minval + (uint) truncf((((float)val) / _chan_cnt) * range);
    if (_scan_monitor)
        _scan_monitor->ScanPercentComplete(pct);
}

void IPTVChannelFetcher::SetMessage(const QString &status)
{
    if (_scan_monitor)
        _scan_monitor->ScanAppendTextToLog(status);
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

    QString rawdata = reallyrawdata;
    rawdata.replace("\r\n", "\n");

    // Verify header is ok
    QString header = rawdata.section("\n", 0, 0);
    if (header != "#EXTM3U")
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

    // Parse each channel
    uint lineNum = 1;
    for (uint i = 1; true; i++)
    {
        IPTVChannelInfo info;
        QString channum = QString::null;

        if (!parse_chan_info(rawdata, info, channum, lineNum))
            break;

        QString msg = tr("Encountered malformed channel");
        if (!channum.isEmpty())
        {
            chanmap[channum] = info;

            msg = QString("Parsing Channel #%1 : %2 : %3")
                .arg(channum).arg(info.m_name)
                .arg(info.m_tuning.GetDataURL().toString());
            LOG(VB_CHANNEL, LOG_INFO, LOC + msg);

            msg = QString::null; // don't tell fetcher
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
                parse_extinf(line.mid(line.indexOf(':')+1), channum, name);
            }
            else if (line.startsWith("#EXTMYTHTV:"))
            {
                QString data = line.mid(line.indexOf(':')+1);
                QString key = data.left(data.indexOf('='));
                if (!key.isEmpty())
                    values[key] = data.mid(data.indexOf('=')+1);
            }
            else if (line.startsWith("#EXTVLCOPT:program="))
            {
                values["programnumber"] = line.mid(line.indexOf('=')+1);
            }
            continue;
        }

        if (name.isEmpty())
            return false;

        QMap<QString,QString>::const_iterator it = values.begin();
        for (; it != values.end(); ++it)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("parse_chan_info [%1]='%2'")
                .arg(it.key()).arg(*it));
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
                         QString       &name)
{
    // Parse extension portion, Freebox or SAT>IP format
    QRegExp chanNumName1("^-?\\d+,(\\d+)(?:\\.\\s|\\s-\\s)(.*)$");
    int pos = chanNumName1.indexIn(line);
    if (pos != -1)
    {
        channum = chanNumName1.cap(1);
        name = chanNumName1.cap(2);
        return true;
    }

    // Parse extension portion, A1 TV format
    QRegExp chanNumName2("^-?\\d+\\s+[^,]*tvg-num=\"(\\d+)\"[^,]*,(.*)$");
    pos = chanNumName2.indexIn(line);
    if (pos != -1)
    {
        channum = chanNumName2.cap(1);
        name = chanNumName2.cap(2);
        return true;
    }

    // Parse extension portion, Moviestar TV number then name
    QRegExp chanNumName3("^-?\\d+,\\[(\\d+)\\]\\s+(.*)$");
    pos = chanNumName3.indexIn(line);
    if (pos != -1)
    {
        channum = chanNumName3.cap(1);
        name = chanNumName3.cap(2);
        return true;
    }

    // Parse extension portion, Moviestar TV name then number
    QRegExp chanNumName4("^-?\\d+,(.*)\\s+\\[(\\d+)\\]$");
    pos = chanNumName4.indexIn(line);
    if (pos != -1)
    {
        channum = chanNumName4.cap(2);
        name = chanNumName4.cap(1);
        return true;
    }

    // Parse extension portion, russion iptv plugin style
    QRegExp chanNumName5("^(-?\\d+)\\s+[^,]*,\\s*(.*)$");
    pos = chanNumName5.indexIn(line);
    if (pos != -1)
    {
        channum = chanNumName5.cap(1).simplified();
        name = chanNumName5.cap(2).simplified();
        bool ok;
        int channel_number = channum.toInt (&ok);
        if ((ok == true) && (channel_number > 0))
        {
            return true;
        }
    }

    // line is supposed to contain the "0,2 - France 2" part
    QString msg = LOC +
        QString("Invalid header in channel list line \n\t\t\tEXTINF:%1")
        .arg(line);
    LOG(VB_GENERAL, LOG_ERR, msg);
    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
