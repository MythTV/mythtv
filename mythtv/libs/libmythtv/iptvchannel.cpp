/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "iptvchannel.h"

// MythTV headers
#include "mythdb.h"
#include "mythlogging.h"
#include "iptvchannelfetcher.h"
#include "iptvfeederwrapper.h"

#define LOC QString("IPTVChan(%1): ").arg(GetCardID())

IPTVChannel::IPTVChannel(TVRec         *parent,
                         const QString &videodev)
    : DTVChannel(parent),
      m_videodev(videodev),
      m_feeder(new IPTVFeederWrapper()),
      m_lock(QMutex::Recursive)
{
    m_videodev.detach();
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
}

IPTVChannel::~IPTVChannel()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor -- begin");
    if (m_feeder)
    {
        delete m_feeder;
        m_feeder = NULL;
    }
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor -- end");
}

bool IPTVChannel::Open(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open() -- begin");
    QMutexLocker locker(&m_lock);
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open() -- locked");

    if (!InitializeInputs())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "InitializeInputs() failed");
        return false;
    }

    if (m_freeboxchannels.empty())
    {
        QString content = IPTVChannelFetcher::DownloadPlaylist(
            m_videodev, true);
        m_freeboxchannels = IPTVChannelFetcher::ParsePlaylist(content);
        LOG(VB_GENERAL, LOG_NOTICE, LOC + QString("Loaded %1 channels from %2")
            .arg(m_freeboxchannels.size()) .arg(m_videodev));
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Open() -- end");
    return !m_freeboxchannels.empty();
}

void IPTVChannel::Close(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close() -- begin");
    QMutexLocker locker(&m_lock);
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close() -- locked");
    //m_freeboxchannels.clear();
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Close() -- end");
}

bool IPTVChannel::IsOpen(void) const
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "IsOpen() -- begin");
    QMutexLocker locker(&m_lock);
    LOG(VB_CHANNEL, LOG_INFO, LOC + "IsOpen() -- locked");
    LOG(VB_CHANNEL, LOG_INFO, LOC + "IsOpen() -- end");
    return !m_freeboxchannels.empty();
}

bool IPTVChannel::SetChannelByString(const QString &channum)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("SetChannelByString(%1) -- begin")
        .arg(channum));
    QMutexLocker locker(&m_lock);
    LOG(VB_CHANNEL, LOG_INFO, LOC + "SetChannelByString() -- locked");

    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    if (it == m_inputs.end())
        return false;

    uint mplexid_restriction;
    if (!IsInputAvailable(m_currentInputID, mplexid_restriction))
        return false;

    // Verify that channel exists
    if (!GetChanInfo(channum).isValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
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

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("SetChannelByString(%1) = %2 -- end")
        .arg(channum).arg(m_curchannelname));
    return true;
}

IPTVChannelInfo IPTVChannel::GetChanInfo(
    const QString &channum, uint sourceid) const
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "GetChanInfo() -- begin");
    QMutexLocker locker(&m_lock);
    LOG(VB_CHANNEL, LOG_INFO, LOC + "GetChanInfo() -- locked");

    IPTVChannelInfo dummy;
    QString msg = LOC + QString("GetChanInfo(%1) failed").arg(channum);

    if (channum.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, msg);
        return dummy;
    }

    if (!sourceid)
    {
        InputMap::const_iterator it = m_inputs.find(m_currentInputID);
        if (it == m_inputs.end())
        {
            LOG(VB_GENERAL, LOG_ERR, msg);
            return dummy;
        }
        sourceid = (*it)->sourceid;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT name "
        "FROM channel "
        "WHERE channum  = :CHANNUM AND "
        "      sourceid = :SOURCEID");

    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("fetching chaninfo", query);
        LOG(VB_GENERAL, LOG_ERR, msg);
        return dummy;
    }

    while (query.next())
    {
        fbox_chan_map_t::const_iterator it;
        it = m_freeboxchannels.find(channum);
        if (it != m_freeboxchannels.end())
        {
            LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                QString("Found: %1").arg((*it).toString()));
            return *it;
        }

        // Try to map name to a channel in the map
        const QString name = query.value(0).toString();
        for (it = m_freeboxchannels.begin();
             it != m_freeboxchannels.end(); ++it)
        {
            if ((*it).m_name == name)
            {
                LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                    QString("Found: %1").arg((*it).toString()));
                return *it;
            }
        }
        LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Not Found"));
    }

    LOG(VB_GENERAL, LOG_ERR, msg);
    return dummy;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
