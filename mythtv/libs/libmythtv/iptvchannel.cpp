/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "iptvchannel.h"

// MythTV headers
#include "mythdb.h"
#include "mythverbose.h"
#include "iptvchannelfetcher.h"
#include "iptvfeederwrapper.h"

#define LOC QString("IPTVChan(%1): ").arg(GetCardID())
#define LOC_ERR QString("IPTVChan(%1), Error: ").arg(GetCardID())

IPTVChannel::IPTVChannel(TVRec         *parent,
                         const QString &videodev)
    : DTVChannel(parent),
      m_videodev(videodev),
      m_feeder(new IPTVFeederWrapper()),
      m_lock(QMutex::Recursive)
{
    m_videodev.detach();
    VERBOSE(VB_CHANNEL, LOC + "ctor");
}

IPTVChannel::~IPTVChannel()
{
    VERBOSE(VB_CHANNEL, LOC + "dtor -- begin");
    if (m_feeder)
    {
        delete m_feeder;
        m_feeder = NULL;
    }
    VERBOSE(VB_CHANNEL, LOC + "dtor -- end");
}

bool IPTVChannel::Open(void)
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
        QString content = IPTVChannelFetcher::DownloadPlaylist(
            m_videodev, true);
        m_freeboxchannels = IPTVChannelFetcher::ParsePlaylist(content);
        VERBOSE(VB_IMPORTANT, LOC + QString("Loaded %1 channels from %2")
            .arg(m_freeboxchannels.size())
            .arg(m_videodev));
    }

    VERBOSE(VB_CHANNEL, LOC + "Open() -- end");
    return !m_freeboxchannels.empty();
}

void IPTVChannel::Close(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Close() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "Close() -- locked");
    //m_freeboxchannels.clear();
    VERBOSE(VB_CHANNEL, LOC + "Close() -- end");
}

bool IPTVChannel::IsOpen(void) const
{
    VERBOSE(VB_CHANNEL, LOC + "IsOpen() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "IsOpen() -- locked");
    VERBOSE(VB_CHANNEL, LOC + "IsOpen() -- end");
    return !m_freeboxchannels.empty();
}

bool IPTVChannel::SetChannelByString(const QString &channum)
{
    VERBOSE(VB_CHANNEL, LOC + "SetChannelByString() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "SetChannelByString() -- locked");

    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    if (it == m_inputs.end())
        return false;

    uint mplexid_restriction;
    if (!IsInputAvailable(m_currentInputID, mplexid_restriction))
        return false;

    // Verify that channel exists
    if (!GetChanInfo(channum).isValid())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("SetChannelByString(%1)").arg(channum) +
                " Invalid channel");
        return false;
    }

    // Set the current channum to the new channel's channum
    QString tmp = channum; tmp.detach();
    m_curchannelname = tmp;

    // Set the dtv channel info for any additional multiplex tuning
    SetDTVInfo(/*atsc_major*/ 0, /*atsc_minor*/ 0,
               /*netid*/ 0,
               /*tsid*/ 0, /*mpeg_prog_num*/ 1);

    HandleScript(channum /* HACK treat channum as freqid */);

    VERBOSE(VB_CHANNEL, LOC + "SetChannelByString() -- end");
    return true;
}

IPTVChannelInfo IPTVChannel::GetChanInfo(
    const QString &channum, uint sourceid) const
{
    VERBOSE(VB_CHANNEL, LOC + "GetChanInfo() -- begin");
    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, LOC + "GetChanInfo() -- locked");

    IPTVChannelInfo dummy;
    QString msg = LOC_ERR + QString("GetChanInfo(%1) failed").arg(channum);

    if (channum.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, msg);
        return dummy;
    }

    if (!sourceid)
    {
        InputMap::const_iterator it = m_inputs.find(m_currentInputID);
        if (it == m_inputs.end())
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
        MythDB::DBError("fetching chaninfo", query);
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
        const QString name = query.value(1).toString();
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
