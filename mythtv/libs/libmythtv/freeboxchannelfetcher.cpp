#include "freeboxchannelfetcher.h"

#include "libmyth/httpcomms.h"
#include "libmyth/mythcontext.h"

#define LOC QString("FBChanFetch: ")
#define LOC_ERR QString("FBChanFetch, Error: ")

QString FreeboxChannelFetcher::DownloadPlaylist(const QString& url)
{
    QString rwUrl(url);
    return QString::fromUtf8(HttpComms::getHttp(rwUrl));
}

static bool parse_chan_info(const QString &line1,
                            QString &channum, QString &name)
{
    // each line contains ://
    // header:extension,channelNum - channelName rtsp://channelUrl
    //#EXTINF:0,2 - France 2 rtsp://mafreebox.freebox.fr/freeboxtv/201

    QString msg = LOC_ERR +
        QString("Invalid header in channel list line \n\t\t\t%1").arg(line1);

    channum = name = QString::null;

    // Verify Line Header
    int pos = line1.find(":", 0);
    if ((pos < 0) || (line1.mid(0, pos) != "#EXTINF"))
    {
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }

    // Parse extension portion
    pos = line1.find(",", pos + 1);
    if (pos < 0)
    {
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }
    //list.push_back(line1.mid(oldPos, pos - oldPos));

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

fbox_chan_map_t FreeboxChannelFetcher::ParsePlaylist(const QString& rawdata)
{
    fbox_chan_map_t chanmap;

    // Verify header is ok
    QString header = rawdata.section("\n", 0, 0);
    if (header != "#EXTM3U")
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Invalid channel list header (%1)").arg(header));

        return chanmap;
    }

    // Parse each channel
    for (int i = 1; true; i += 2)
    {
        QString tmp = rawdata.section("\n", i+0, i+0);
        QString url = rawdata.section("\n", i+1, i+1);
        if (tmp.isEmpty() || url.isEmpty())
            break;

        QString channum, name;
        if (parse_chan_info(tmp, channum, name))
        {
            chanmap[channum] = FreeboxChannelInfo(name, url);
        }
    }

    return chanmap;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
