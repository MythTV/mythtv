// -*- Mode: c++ -*-

// Std C headers
#include <cmath>
#include <unistd.h>

// Qt headers
#include <QFile>
#include <QTextStream>

// MythTV headers
#include "mythcontext.h"
#include "httpcomms.h"
#include "mythverbose.h"
#include "cardutil.h"
#include "channelutil.h"
#include "iptvchannelfetcher.h"
#include "scanmonitor.h"
#include "mythlogging.h"

#define LOC QString("IPTVChanFetch: ")
#define LOC_ERR QString("IPTVChanFetch, Error: ")

static bool parse_chan_info(const QString   &rawdata,
                            IPTVChannelInfo &info,
                            QString         &channum,
                            uint            &lineNum);

static bool parse_extinf(const QString &data,
                         QString       &channum,
                         QString       &name);

/** \fn IPTVChannelFetcherThread::run(void)
*   \brief Thunk that allows iptvfetcher Qthread to
*         call IPTVChannelFetcher::RunScan().
*/
void IPTVChannelFetcherThread::run(void)
{
    threadRegister("IPTVChannelFetcher");
    iptvfetcher->RunScan();
    threadDeregister();
}

IPTVChannelFetcher::IPTVChannelFetcher(
    uint cardid, const QString &inputname, uint sourceid,
    ScanMonitor *monitor) :
    _scan_monitor(monitor),
    _cardid(cardid),       _inputname(inputname),
    _sourceid(sourceid),
    _chan_cnt(1),          _thread_running(false),
    _stop_now(false),      _lock()
{
    _inputname.detach();
}

IPTVChannelFetcher::~IPTVChannelFetcher()
{
    do
    {
        Stop();
        usleep(5000);
    }
    while (_thread_running);
}

/** \fn IPTVChannelFetcher::Stop(void)
 *  \brief Stops the scanning thread running
 */
void IPTVChannelFetcher::Stop(void)
{
    _lock.lock();

    if (_thread_running)
    {
        _stop_now = true;
        _lock.unlock();

        _thread.wait();
        return;
    }

    _lock.unlock();
}

/** \fn IPTVChannelFetcher::Scan(void)
 *  \brief Scans the given frequency list, blocking call.
 */
bool IPTVChannelFetcher::Scan(void)
{
    _lock.lock();
    do { _lock.unlock(); Stop(); _lock.lock(); } while (_thread_running);

    // Should now have _lock and no thread should be running.

    _stop_now = false;

    _thread.iptvfetcher = this;
    _thread.start(QThread::NormalPriority);

    while (!_thread_running && !_stop_now)
        usleep(5000);

    _lock.unlock();

    return _thread_running;
}

void IPTVChannelFetcher::RunScan(void)
{
    _thread_running = true;

    // Step 1/4 : Get info from DB
    QString url = CardUtil::GetVideoDevice(_cardid);

    if (_stop_now || url.isEmpty())
    {
        _thread_running = false;
        return;
    }

    VERBOSE(VB_CHANNEL, QString("Playlist URL: %1").arg(url));

    // Step 2/4 : Download
    if (_scan_monitor)
    {
        _scan_monitor->ScanPercentComplete(5);
        _scan_monitor->ScanAppendTextToLog(QObject::tr("Downloading Playlist"));
    }

    QString playlist = DownloadPlaylist(url, true);

    if (_stop_now || playlist.isEmpty())
    {
        _thread_running = false;
        return;
    }

    // Step 3/4 : Process
    if (_scan_monitor)
    {
        _scan_monitor->ScanPercentComplete(35);
        _scan_monitor->ScanAppendTextToLog(QObject::tr("Processing Playlist"));
    }

    const fbox_chan_map_t channels = ParsePlaylist(playlist, this);

    // Step 4/4 : Finish up
    if (_scan_monitor)
        _scan_monitor->ScanAppendTextToLog(QObject::tr("Adding Channels"));
    SetTotalNumChannels(channels.size());
    fbox_chan_map_t::const_iterator it = channels.begin();
    for (uint i = 1; it != channels.end(); ++it, ++i)
    {
        QString channum = it.key();
        QString name    = (*it).m_name;
        QString xmltvid = (*it).m_xmltvid.isEmpty() ? "" : (*it).m_xmltvid;
        QString msg = QObject::tr("Channel #%1 : %2").arg(channum).arg(name);

        int chanid = ChannelUtil::GetChanID(_sourceid, channum);
        if (chanid <= 0)
        {
            if (_scan_monitor)
            {
                _scan_monitor->ScanAppendTextToLog(
                    QObject::tr("Adding %1").arg(msg));
            }
            chanid = ChannelUtil::CreateChanID(_sourceid, channum);
            ChannelUtil::CreateChannel(
                0, _sourceid, chanid, name, name, channum,
                0, 0, 0, false, false, false, QString::null,
                QString::null, "Default", xmltvid);
        }
        else
        {
            if (_scan_monitor)
            {
                _scan_monitor->ScanAppendTextToLog(
                    QObject::tr("Updating %1").arg(msg));
            }
            ChannelUtil::UpdateChannel(
                0, _sourceid, chanid, name, name, channum,
                0, 0, 0, false, false, false, QString::null,
                QString::null, "Default", xmltvid);
        }

        SetNumChannelsInserted(i);
    }

    if (_scan_monitor)
    {
        _scan_monitor->ScanAppendTextToLog(QObject::tr("Done"));
        _scan_monitor->ScanAppendTextToLog("");
        _scan_monitor->ScanPercentComplete(100);
        _scan_monitor->ScanComplete();
    }

    _thread_running = false;
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

QString IPTVChannelFetcher::DownloadPlaylist(const QString &url,
                                             bool inQtThread)
{
    if (url.left(4).toLower() == "file")
    {
        QString ret = "";
        QUrl qurl(url);
        QFile file(qurl.toLocalFile());
        if (!file.open(QIODevice::ReadOnly))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Opening '%1'")
                    .arg(qurl.toLocalFile()) + ENO);
            return ret;
        }

        QTextStream stream(&file);
        while (!stream.atEnd())
            ret += stream.readLine() + "\n";

        file.close();
        return ret;
    }

    // Use Myth HttpComms for http URLs
    QString redirected_url = url;

    QString tmp = HttpComms::getHttp(
        redirected_url,
        10000 /* ms        */, 3     /* retries      */,
        3     /* redirects */, true  /* allow gzip   */,
        NULL  /* login     */, inQtThread);

    if (redirected_url != url)
    {
        VERBOSE(VB_CHANNEL, QString("Channel URL redirected to %1")
                .arg(redirected_url));
    }

    return QString::fromUtf8(tmp.toAscii().constData());
}

static uint estimate_number_of_channels(const QString &rawdata)
{
    uint result = 0;
    uint numLine = 1;
    while (true)
    {
        QString url = rawdata.section("\n", numLine, numLine);
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Invalid channel list header (%1)").arg(header));

        if (fetcher)
        {
            fetcher->SetMessage(
                QObject::tr("ERROR: M3U channel list is malformed"));
        }

        return chanmap;
    }

    // estimate number of channels
    if (fetcher)
    {
        uint num_channels = estimate_number_of_channels(rawdata);
        fetcher->SetTotalNumChannels(num_channels);

        VERBOSE(VB_CHANNEL, QString("Estimating there are %1"
                " channels in playlist").arg(num_channels));
    }

    // Parse each channel
    uint lineNum = 1;
    for (uint i = 1; true; i++)
    {
        IPTVChannelInfo info;
        QString channum = QString::null;

        if (!parse_chan_info(rawdata, info, channum, lineNum))
            break;

        QString msg = QObject::tr("Encountered malformed channel");
        if (!channum.isEmpty())
        {
            chanmap[channum] = info;

            msg = QObject::tr("Parsing Channel #%1 : %2 : %3")
                .arg(channum).arg(info.m_name).arg(info.m_url);
            VERBOSE(VB_CHANNEL, msg);

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
    // #...                                  <-- ignored comments
    // rtsp://maiptv.iptv.fr/iptvtv/201 <-- url

    QString name;
    QString xmltvid;
    while (true)
    {
        QString line = rawdata.section("\n", lineNum, lineNum);
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
                if (data.startsWith("xmltvid="))
                {
                    xmltvid = data.mid(data.indexOf('=')+1);
                }
            }
            else
            {
                // Just ignore other comments
            }
        }
        else
        {
            if (name.isEmpty())
                return false;
            QString url = line;
            info = IPTVChannelInfo(name, url, xmltvid);
            return true;
        }
    }
}

static bool parse_extinf(const QString &line1,
                         QString       &channum,
                         QString       &name)
{
    // data is supposed to contain the "0,2 - France 2" part
    QString msg = LOC_ERR +
        QString("Invalid header in channel list line \n\t\t\tEXTINF:%1")
        .arg(line1);

    // Parse extension portion
    int pos = line1.indexOf(",");
    if (pos < 0)
    {
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    // Parse iptv channel number
    int oldpos = pos + 1;
    pos = line1.indexOf(" ", pos + 1);
    if (pos < 0)
    {
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }
    channum = line1.mid(oldpos, pos - oldpos);

    // Parse iptv channel name
    pos = line1.indexOf("- ", pos + 1);
    if (pos < 0)
    {
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }
    name = line1.mid(pos + 2, line1.length());

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
