// -*- Mode: c++ -*-

// Std C headers
#include <cmath>
#include <unistd.h>

// MythTV headers
#include "mythcontext.h"
#include "httpcomms.h"
#include "cardutil.h"
#include "channelutil.h"
#include "freeboxchannelfetcher.h"

#define LOC QString("FBChanFetch: ")
#define LOC_ERR QString("FBChanFetch, Error: ")

static bool parse_chan_info(const QString      &rawdata,
                            FreeboxChannelInfo &info,
                            QString            &channum,
                            uint               &lineNum);
static bool parse_extinf(const QString &data,
                         QString &channum, QString &name);

FreeboxChannelFetcher::FreeboxChannelFetcher(unsigned _sourceid,
                                             unsigned _cardid) :
    sourceid(_sourceid),    cardid(_cardid),
    chan_cnt(1),            thread_running(false),
    stop_now(false),        lock(false)
{
}

FreeboxChannelFetcher::~FreeboxChannelFetcher()
{
    do
    {
        Stop();
        usleep(5000);
    }
    while (thread_running);
}

void FreeboxChannelFetcher::Stop(void)
{
    lock.lock();

    if (thread_running)
    {
        stop_now = true;
        lock.unlock();

        pthread_join(thread, NULL);
        return;
    }

    lock.unlock();
}

void *run_scan_thunk(void *param)
{
    FreeboxChannelFetcher *chanscan = (FreeboxChannelFetcher*) param;
    chanscan->RunScan();

    return NULL;
}

bool FreeboxChannelFetcher::Scan(void)
{
    lock.lock();
    do { lock.unlock(); Stop(); lock.lock(); } while (thread_running);

    // Should now have lock and no thread should be running.

    stop_now = false;

    pthread_create(&thread, NULL, run_scan_thunk, this);

    while (!thread_running && !stop_now)
        usleep(5000);

    lock.unlock();

    return thread_running;
}

void FreeboxChannelFetcher::RunScan(void)
{
    thread_running = true;

    // Step 1/4 : Get info from DB
    QString url = CardUtil::GetVideoDevice(cardid, sourceid);

    if (stop_now || url.isEmpty())
    {
        thread_running = false;
        return;
    }

    VERBOSE(VB_CHANNEL, QString("Playlist URL: %1").arg(url));

    // Step 2/4 : Download
    emit ServiceScanPercentComplete(5);
    emit ServiceScanUpdateText(tr("Downloading Playlist"));

    QString playlist = DownloadPlaylist(url, false);

    if (stop_now || playlist.isEmpty())
    {
        thread_running = false;
        return;
    }

    // Step 3/4 : Process
    emit ServiceScanPercentComplete(35);
    emit ServiceScanUpdateText(tr("Processing Playlist"));

    const fbox_chan_map_t channels = ParsePlaylist(playlist, this);

    // Step 4/4 : Finish up
    emit ServiceScanUpdateText(tr("Adding Channels"));
    SetTotalNumChannels(channels.size());
    fbox_chan_map_t::const_iterator it = channels.begin();    
    for (uint i = 1; it != channels.end(); ++it, ++i)
    {
        QString channum = it.key();
        QString name    = (*it).m_name;
        QString xmltvid = (*it).m_xmltvid.isEmpty() ? "" : (*it).m_xmltvid;
        QString msg     = tr("Channel #%1 : %2").arg(channum).arg(name);

        int chanid = ChannelUtil::GetChanID(sourceid, channum);
        if (chanid <= 0)
        { 
            emit ServiceScanUpdateText(tr("Adding %1").arg(msg));
            chanid = ChannelUtil::CreateChanID(sourceid, channum);
            ChannelUtil::CreateChannel(
                0, sourceid, chanid, name, name, channum,
                0, 0, 0, false, false, false, 0,
                "", "Default", xmltvid);
        }
        else
        {
            emit ServiceScanUpdateText(tr("Updating %1").arg(msg));
            ChannelUtil::UpdateChannel(
                0, sourceid, chanid, name, name, channum, 0, 0, 0, 0);
            //TODO Update the xmltvid
        }

        SetNumChannelsInserted(i);
    }

    emit ServiceScanUpdateText(tr("Done"));
    emit ServiceScanUpdateText("");
    emit ServiceScanPercentComplete(100);
    emit ServiceScanComplete();

    thread_running = false;
}

void FreeboxChannelFetcher::SetNumChannelsParsed(uint val)
{
    uint minval = 35, range = 70 - minval;
    uint pct = minval + (uint) truncf((((float)val) / chan_cnt) * range);
    emit ServiceScanPercentComplete(pct);
}

void FreeboxChannelFetcher::SetNumChannelsInserted(uint val)
{
    uint minval = 70, range = 100 - minval;
    uint pct = minval + (uint) truncf((((float)val) / chan_cnt) * range);
    emit ServiceScanPercentComplete(pct);
}

void FreeboxChannelFetcher::SetMessage(const QString &status)
{
    emit ServiceScanUpdateText(status);
}

QString FreeboxChannelFetcher::DownloadPlaylist(const QString &url,
                                                bool inQtThread)
{
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

    return QString::fromUtf8(tmp);
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

fbox_chan_map_t FreeboxChannelFetcher::ParsePlaylist(
    const QString &rawdata, FreeboxChannelFetcher *fetcher)
{
    fbox_chan_map_t chanmap;

    // Verify header is ok
    QString header = rawdata.section("\n", 0, 0);
    if (header != "#EXTM3U")
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Invalid channel list header (%1)").arg(header));

        if (fetcher)
            fetcher->SetMessage(tr("ERROR: M3U channel list is malformed"));

        return chanmap;
    }

    // estimate number of channels
    if (fetcher)
    {
        uint num_channels = estimate_number_of_channels(rawdata);
        fetcher->SetTotalNumChannels(num_channels);

        VERBOSE(VB_CHANNEL, "Estimating there are "<<num_channels
                <<" channels in playlist");
    }

    // Parse each channel
    uint lineNum = 1;
    for (uint i = 1; true; i++)
    {
        FreeboxChannelInfo info;
        QString channum = QString::null;

        if (!parse_chan_info(rawdata, info, channum, lineNum))
            break;

        QString msg = tr("Encountered malformed channel");
        if (!channum.isEmpty())
        {
            chanmap[channum] = info;

            msg = tr("Parsing Channel #%1 : %2 : %3")
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

static bool parse_chan_info(const QString      &rawdata,
                            FreeboxChannelInfo &info,
                            QString            &channum,
                            uint               &lineNum)
{
    // #EXTINF:0,2 - France 2                <-- duration,channum - channame
    // #EXTMYTHTV:xmltvid=C2.telepoche.com   <-- optional line (myth specific)
    // #...                                  <-- ignored comments
    // rtsp://mafreebox.freebox.fr/freeboxtv/201 <-- url

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
                parse_extinf(line.mid(line.find(':')+1), channum, name);
            }
            else if (line.startsWith("#EXTMYTHTV:"))
            {
                QString data = line.mid(line.find(':')+1);
                if (data.startsWith("xmltvid="))
                {
                    xmltvid = data.mid(data.find('=')+1);
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
            info = FreeboxChannelInfo(name, url, xmltvid);
            return true;
        }
    }
}

static bool parse_extinf(const QString &line1,
                         QString &channum, QString &name)
{
    // data is supposed to contain the "0,2 - France 2" part
    QString msg = LOC_ERR +
        QString("Invalid header in channel list line \n\t\t\tEXTINF:%1")
        .arg(line1);

    // Parse extension portion
    int pos = line1.find(",");
    if (pos < 0)
    {
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    // Parse freebox channel number
    int oldpos = pos + 1;
    pos = line1.find(" ", pos + 1);
    if (pos < 0)
    {
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }
    channum = line1.mid(oldpos, pos - oldpos);

    // Parse freebox channel name
    pos = line1.find("- ", pos + 1);
    if (pos < 0)
    {
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }
    name = line1.mid(pos + 2, line1.length());

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
