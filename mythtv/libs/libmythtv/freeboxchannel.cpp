/** -*- Mode: c++ -*-
 *  FreeboxChannel
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "freeboxchannel.h"

#include <qdeepcopy.h>

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmythtv/freeboxchannelfetcher.h"
#include "libmythtv/rtspcomms.h"

#define LOC QString("FBChan(%1): ").arg(GetCardID())
#define LOC_ERR QString("FBChan(%1), Error: ").arg(GetCardID())

FreeboxChannel::FreeboxChannel(TVRec         *parent,
                               const QString &videodev)
    : ChannelBase(parent),
      m_videodev(QDeepCopy<QString>(videodev)),
      m_rtsp(new RTSPComms()),
      m_lock(true)
{
    VERBOSE(VB_CHANNEL, LOC + "ctor");
}

FreeboxChannel::~FreeboxChannel()
{
    VERBOSE(VB_CHANNEL, LOC + "dtor -- begin");
    if (m_rtsp)
    {
        delete m_rtsp;
        m_rtsp = NULL;
    }
    VERBOSE(VB_CHANNEL, LOC + "dtor -- end");
}

bool FreeboxChannel::Open(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Open() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "Open() -- locked");

    if (!InitializeInputs())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "InitializeInputs() failed");
        return false;
    }
    
    if (m_freeboxchannels.empty())
    {
        QString content = FreeboxChannelFetcher::DownloadPlaylist(
            m_videodev, true);
        m_freeboxchannels = FreeboxChannelFetcher::ParsePlaylist(content);
        VERBOSE(VB_IMPORTANT, LOC + QString("Loaded %1 channels from %2")
            .arg(m_freeboxchannels.size())
            .arg(m_videodev));
    }

    bool open = IsOpen();
    VERBOSE(VB_CHANNEL, LOC + "Open() -- end");
    return open;
}

void FreeboxChannel::Close(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Close() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "Close() -- locked");
    m_freeboxchannels.clear();
    VERBOSE(VB_CHANNEL, LOC + "Close() -- end");
}

bool FreeboxChannel::IsOpen(void) const
{
    VERBOSE(VB_CHANNEL, LOC + "IsOpen() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "IsOpen() -- locked");
    bool open = m_freeboxchannels.size() > 0;
    VERBOSE(VB_CHANNEL, LOC + "IsOpen() -- end");
    return open;
}

bool FreeboxChannel::SwitchToInput(const QString &inputname,
                                   const QString &channum)
{
    VERBOSE(VB_CHANNEL, LOC + QString("SwitchToInput(%1)").arg(inputname));
    QMutexLocker locker(&m_lock);

    int inputNum = GetInputByName(inputname);
    if (inputNum < 0)
        return false;

    return SetChannelByString(channum);
}

bool FreeboxChannel::SwitchToInput(int inputNum, bool setstarting)
{
    VERBOSE(VB_CHANNEL, LOC + QString("SwitchToInput(%1)").arg(inputNum));
    QMutexLocker locker(&m_lock);

    InputMap::const_iterator it = inputs.find(inputNum);
    if (it == inputs.end())
        return false;

    QString channum = (*it)->startChanNum;

    if (setstarting)
        return SetChannelByString(channum);

    return true;
}

bool FreeboxChannel::SetChannelByString(const QString &channum)
{
    VERBOSE(VB_CHANNEL, LOC + "SetChannelByString() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "SetChannelByString() -- locked");

    // Verify that channel exists
    if (!GetChanInfo(channum).isValid())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("SetChannelByString(%1)").arg(channum) +
                " Invalid channel");
        return false;
    }

    // Set the channel..
    curchannelname = channum;
    currentProgramNum = 1;

    VERBOSE(VB_CHANNEL, LOC + "SetChannelByString() -- end");
    return true;
}

FreeboxChannelInfo FreeboxChannel::GetChanInfo(const QString &channum,
                                               uint           sourceid) const
{ 
    VERBOSE(VB_CHANNEL, LOC + "GetChanInfo() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "GetChanInfo() -- locked");

    FreeboxChannelInfo dummy;
    QString msg = LOC_ERR + QString("GetChanInfo(%1) failed").arg(channum);

    if (channum.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, msg);
        return dummy;
    }

    if (!sourceid)
    {
        InputMap::const_iterator it = inputs.find(currentInputID);
        if (it == inputs.end())
        {
            VERBOSE(VB_IMPORTANT, msg);
            return dummy;
        }
        sourceid = (*it)->sourceid;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT freqid, name "
        "FROM channel "
        "WHERE channum  = :CHANNUM AND "
        "      sourceid = :SOURCEID");

    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("fetching chaninfo", query);
        VERBOSE(VB_IMPORTANT, msg);
        return dummy;
    }

    while (query.next())
    {
        // Try to map freqid or name to a channel in the map
        const QString freqid = query.value(0).toString();
        fbox_chan_map_t::const_iterator it;
        if (!freqid.isEmpty())
        {
            it = m_freeboxchannels.find(freqid);
            if (it != m_freeboxchannels.end())
                return *it;
        }

        // Try to map name to a channel in the map
        const QString name    = query.value(1).toString();
        for (it = m_freeboxchannels.begin();
             it != m_freeboxchannels.end(); ++it)
        {
            if ((*it).m_name == name)
                return *it;
        }
    }

    VERBOSE(VB_IMPORTANT, msg);
    return dummy;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
