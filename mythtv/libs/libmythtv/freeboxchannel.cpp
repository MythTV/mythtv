/** -*- Mode: c++ -*-
 *  FreeboxChannel
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "freeboxchannel.h"

#include "libmythtv/freeboxchannelfetcher.h"
#include "libmythtv/freeboxrecorder.h"
#include "libmythtv/tv_rec.h"

#define LOC QString("FBChan(%1): ").arg(GetCardID())
#define LOC_ERR QString("FBChan(%1), Error: ").arg(GetCardID())

FreeboxChannel::FreeboxChannel(TVRec         *parent,
                               const QString &videodev)
    : ChannelBase(parent),
      m_videodev(QDeepCopy<QString>(videodev)),
      m_recorder(NULL),
      m_lock(true)
{
}

bool FreeboxChannel::Open(void)
{
    QMutexLocker locker(&m_lock);

    if (!InitializeInputs())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "InitializeInputs() failed");
        return false;
    }
    
    if (m_freeboxchannels.empty())
    {
        QString content = FreeboxChannelFetcher::DownloadPlaylist(m_videodev);
        m_freeboxchannels = FreeboxChannelFetcher::ParsePlaylist(content);
        VERBOSE(VB_IMPORTANT, LOC + QString("Loaded %1 channels from %2")
            .arg(m_freeboxchannels.size())
            .arg(m_videodev));
    }

    return IsOpen();
}

void FreeboxChannel::Close(void)
{
    QMutexLocker locker(&m_lock);
    m_freeboxchannels.clear();
}

bool FreeboxChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_lock);
    return m_freeboxchannels.size() > 0;
}

bool FreeboxChannel::SwitchToInput(const QString &inputname,
                                   const QString &channum)
{
    QMutexLocker locker(&m_lock);

    int inputNum = GetInputByName(inputname);
    if (inputNum < 0)
        return false;

    return SetChannelByString(channum);
}

bool FreeboxChannel::SwitchToInput(int inputNum, bool setstarting)
{
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
    QMutexLocker locker(&m_lock);

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

    // Send signal to recorder that channel has changed.
    if (m_recorder)
    {
        FreeboxChannelInfo chaninfo(GetCurrentChanInfo());
        m_recorder->ChannelChanged(chaninfo);
    }

    return true;
}

FreeboxChannelInfo FreeboxChannel::GetChanInfo(const QString &channum,
                                               uint           sourceid) const
{
    QMutexLocker locker(&m_lock);

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

void FreeboxChannel::SetRecorder(FreeboxRecorder *rec)
{
    QMutexLocker locker(&m_lock);
    m_recorder = rec;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
