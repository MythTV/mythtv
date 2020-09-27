// Qt
#include <QCoreApplication>

// MythTV
#include "mythcorecontext.h"
#include "tvbrowsehelper.h"
#include "playercontext.h"
#include "remoteencoder.h"
#include "recordinginfo.h"
#include "channelutil.h"
#include "mythlogging.h"
#include "cardutil.h"
#include "tv_play.h"

#define LOC QString("BrowseHelpers: ")

static void format_time(int seconds, QString& tMin, QString& tHrsMin)
{
    int minutes     = seconds / 60;
    int hours       = minutes / 60;
    int min         = minutes % 60;

    tMin = TV::tr("%n minute(s)", "", minutes);
    tHrsMin = QString("%1:%2").arg(hours).arg(min, 2, 10, QChar('0'));
}

TVBrowseHelper::TVBrowseHelper(TV* Tv, uint BrowseMaxForward, bool BrowseAllTuners,
                               bool UseChannelGroups, const QString& DBChannelOrdering)
  : MThread("TVBrowseHelper"),
    m_tv(Tv),
    m_dbBrowseMaxForward(BrowseMaxForward),
    m_dbBrowseAllTuners(BrowseAllTuners),
    m_dbUseChannelGroups(UseChannelGroups)
{
    m_dbAllChannels = ChannelUtil::GetChannels(0, true, "channum, callsign");
    ChannelUtil::SortChannels(m_dbAllChannels, DBChannelOrdering, false);

    for (const auto & chan : m_dbAllChannels)
    {
        m_dbChanidToChannum[chan.m_chanId] = chan.m_chanNum;
        m_dbChanidToSourceid[chan.m_chanId] = chan.m_sourceId;
        m_dbChannumToChanids.insert(chan.m_chanNum,chan.m_chanId);
    }

    m_dbAllVisibleChannels = ChannelUtil::GetChannels(0, true, "channum, callsign");
    ChannelUtil::SortChannels(m_dbAllVisibleChannels, DBChannelOrdering, false);
    start();
}

TVBrowseHelper::~TVBrowseHelper()
{
    Stop();
    Wait();
}

void TVBrowseHelper::Stop()
{
    QMutexLocker locker(&m_lock);
    m_list.clear();
    m_run = false;
    m_wait.wakeAll();
}

void TVBrowseHelper::Wait()
{
    MThread::wait();
}

/// \brief Begins channel browsing.
/// \note This may only be called from the UI thread.
bool TVBrowseHelper::BrowseStart(PlayerContext* Ctx, bool SkipBrowse)
{
    if (!gCoreContext->IsUIThread())
        return false;

    QMutexLocker locker(&m_lock);

    if (m_ctx)
        return m_ctx == Ctx;

    m_tv->ClearOSD();

    Ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (Ctx->m_playingInfo)
    {
        m_ctx       = Ctx;
        m_chanNum   = Ctx->m_playingInfo->GetChanNum();
        m_chanId    = Ctx->m_playingInfo->GetChanID();
        m_startTime = Ctx->m_playingInfo->GetScheduledStartTime(MythDate::ISODate);
        Ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        if (!SkipBrowse)
        {
            BrowseInfo bi(BROWSE_SAME, m_chanNum, m_chanId, m_startTime);
            locker.unlock();
            BrowseDispInfo(Ctx, bi);
        }
        return true;
    }
    Ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    return false;
}

/** \brief Ends channel browsing.
 *         Changing the channel if change_channel is true.
 *  \note This may only be called from the UI thread.
 *  \param ctx PlayerContext to end browsing on
 *  \param change_channel iff true we call ChangeChannel()
 */
void TVBrowseHelper::BrowseEnd(PlayerContext* Ctx, bool ChangeChannel)
{
    if (!gCoreContext->IsUIThread())
        return;

    QMutexLocker locker(&m_lock);

    if (Ctx && m_ctx != Ctx)
        return;

    if (!m_ctx)
        return;

    {
        QMutexLocker locker2(&m_tv->m_timerIdLock);
        if (m_tv->m_browseTimerId)
        {
            m_tv->KillTimer(m_tv->m_browseTimerId);
            m_tv->m_browseTimerId = 0;
        }
    }

    m_list.clear();
    m_wait.wakeAll();

    OSD* osd = m_tv->GetOSDL();
    if (osd)
        osd->HideWindow("browse_info");
    m_tv->ReturnOSDLock();

    if (ChangeChannel)
        m_tv->ChangeChannel(0, m_chanNum);

    m_ctx = nullptr;
}

void TVBrowseHelper::BrowseDispInfo(PlayerContext* Ctx, const BrowseInfo& Browseinfo)
{
    if (!gCoreContext->IsUIThread())
        return;

    if (!BrowseStart(Ctx, true))
        return;

    {
        QMutexLocker locker(&m_tv->m_timerIdLock);
        if (m_tv->m_browseTimerId)
        {
            m_tv->KillTimer(m_tv->m_browseTimerId);
            m_tv->m_browseTimerId =
                m_tv->StartTimer(static_cast<int>(TV::kBrowseTimeout), __LINE__);
        }
    }

    QMutexLocker locker(&m_lock);
    if (BROWSE_SAME == Browseinfo.m_dir)
        m_list.clear();
    m_list.push_back(Browseinfo);
    m_wait.wakeAll();
}

void TVBrowseHelper::BrowseDispInfo(PlayerContext* Ctx, BrowseDirection Direction)
{
    BrowseInfo bi(Direction);
    if (BROWSE_SAME != Direction)
        BrowseDispInfo(Ctx, bi);
}

void TVBrowseHelper::BrowseChannel(PlayerContext* Ctx, const QString& Channum)
{
    if (!gCoreContext->IsUIThread())
        return;

    if (m_dbBrowseAllTuners)
    {
        BrowseInfo bi(Channum, 0);
        BrowseDispInfo(Ctx, bi);
        return;
    }

    if (!Ctx->m_recorder || !Ctx->m_lastCardid)
        return;

    uint inputid  = static_cast<uint>(Ctx->m_lastCardid);
    uint sourceid = CardUtil::GetSourceID(inputid);
    if (sourceid)
    {
        BrowseInfo bi(Channum, sourceid);
        BrowseDispInfo(Ctx, bi);
    }
}

BrowseInfo TVBrowseHelper::GetBrowsedInfo() const
{
    QMutexLocker locker(&m_lock);
    BrowseInfo bi(BROWSE_SAME);
    if (m_ctx != nullptr)
    {
        bi.m_chanNum   = m_chanNum;
        bi.m_chanId    = m_chanId;
        bi.m_startTime = m_startTime;
    }
    return bi;
}

/**
 *  \note This may only be called from the UI thread.
 */
bool TVBrowseHelper::IsBrowsing() const
{
    if (!gCoreContext->IsUIThread())
        return true;

    return m_ctx != nullptr;
}

/** \brief Returns a chanid for the channum, or 0 if none is available.
 *
 *  This will prefer a given sourceid first, and then a given card id,
 *  but if one or the other can not be satisfied but m_dbBrowseAllTuners
 *  is set then it will look to see if the chanid is available on any
 *  tuner.
 */
uint TVBrowseHelper::GetChanId(const QString& Channum, uint PrefCardid, uint PrefSourceid) const
{
    if (PrefSourceid)
        for (const auto & chan : m_dbAllChannels)
            if (chan.m_sourceId == PrefSourceid && chan.m_chanNum == Channum)
                return chan.m_chanId;

    if (PrefCardid)
        for (const auto & chan : m_dbAllChannels)
            if (chan.GetInputIds().contains(PrefCardid) && chan.m_chanNum == Channum)
                return chan.m_chanId;

    if (m_dbBrowseAllTuners)
    {
        for (const auto & chan : m_dbAllChannels)
            if (chan.m_chanNum == Channum)
                return chan.m_chanId;
    }

    return 0;
}

/**
 *  \brief Fetches information on the desired program from the backend.
 *  \param direction BrowseDirection to get information on.
 *  \param infoMap InfoMap to fill in with returned data
 */
void TVBrowseHelper::GetNextProgram(BrowseDirection Direction, InfoMap& Infomap) const
{
    if (!m_ctx || !m_ctx->m_recorder)
        return;

    QString title;
    QString subtitle;
    QString desc;
    QString category;
    QString endtime;
    QString callsign;
    QString iconpath;
    QDateTime begts;
    QDateTime endts;

    QString starttime = Infomap["dbstarttime"];
    QString chanid    = Infomap["chanid"];
    QString channum   = Infomap["channum"];
    QString seriesid  = Infomap["seriesid"];
    QString programid = Infomap["programid"];

    m_ctx->m_recorder->GetNextProgram(Direction, title, subtitle, desc, category,
                                      starttime, endtime, callsign, iconpath,
                                      channum, chanid, seriesid, programid);

    if (!starttime.isEmpty())
        begts = MythDate::fromString(starttime);
    else
        begts = MythDate::fromString(Infomap["dbstarttime"]);

    Infomap["starttime"] = MythDate::toString(begts, MythDate::kTime);
    Infomap["startdate"] = MythDate::toString(begts, MythDate::kDateFull | MythDate::kSimplify);

    Infomap["endtime"] = Infomap["enddate"] = "";
    if (!endtime.isEmpty())
    {
        endts = MythDate::fromString(endtime);
        Infomap["endtime"] = MythDate::toString(endts, MythDate::kTime);
        Infomap["enddate"] = MythDate::toString(endts, MythDate::kDateFull | MythDate::kSimplify);
    }

    Infomap["lenmins"] = TV::tr("%n minute(s)", "", 0);
    Infomap["lentime"] = "0:00";
    if (begts.isValid() && endts.isValid())
    {
        QString lenM;
        QString lenHM;
        format_time(static_cast<int>(begts.secsTo(endts)), lenM, lenHM);
        Infomap["lenmins"] = lenM;
        Infomap["lentime"] = lenHM;
    }

    Infomap["dbstarttime"] = starttime;
    Infomap["dbendtime"]   = endtime;
    Infomap["title"]       = title;
    Infomap["subtitle"]    = subtitle;
    Infomap["description"] = desc;
    Infomap["category"]    = category;
    Infomap["callsign"]    = callsign;
    Infomap["channum"]     = channum;
    Infomap["chanid"]      = chanid;
    Infomap["iconpath"]    = iconpath;
    Infomap["seriesid"]    = seriesid;
    Infomap["programid"]   = programid;
}

void TVBrowseHelper::GetNextProgramDB(BrowseDirection direction, InfoMap& Infomap) const
{
    uint chanid = Infomap["chanid"].toUInt();
    if (!chanid)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetNextProgramDB() requires a chanid");
        return;
    }

    int chandir = -1;
    switch (direction)
    {
        case BROWSE_UP:       chandir = CHANNEL_DIRECTION_UP;       break;
        case BROWSE_DOWN:     chandir = CHANNEL_DIRECTION_DOWN;     break;
        case BROWSE_FAVORITE: chandir = CHANNEL_DIRECTION_FAVORITE; break;
        case BROWSE_SAME:
        case BROWSE_LEFT:
        case BROWSE_RIGHT:
        case BROWSE_INVALID: break;
    }

    if (chandir != -1)
    {
        chanid = ChannelUtil::GetNextChannel(m_dbAllVisibleChannels, chanid, 0 /*mplexid_restriction*/,
                                             0 /* chanid restriction */,
                                             static_cast<ChannelChangeDirection>(chandir),
                                             true /*skip non visible*/, true /*skip same callsign*/);
    }

    Infomap["chanid"]  = QString::number(chanid);
    Infomap["channum"] = m_dbChanidToChannum[chanid];

    QDateTime nowtime = MythDate::current();
    QDateTime latesttime = nowtime.addSecs(6*60*60);
    QDateTime browsetime = MythDate::fromString(Infomap["dbstarttime"]);

    MSqlBindings bindings;
    bindings[":CHANID"] = chanid;
    bindings[":NOWTS"] = nowtime;
    bindings[":LATESTTS"] = latesttime;
    bindings[":BROWSETS"] = browsetime;
    bindings[":BROWSETS2"] = browsetime;

    QString querystr = " WHERE program.chanid = :CHANID ";
    switch (direction)
    {
        case BROWSE_LEFT:
            querystr += " AND program.endtime <= :BROWSETS AND program.endtime > :NOWTS ";
            break;
        case BROWSE_RIGHT:
            querystr += " AND program.starttime > :BROWSETS AND program.starttime < :LATESTTS ";
            break;
        default:
            querystr += " AND program.starttime <= :BROWSETS AND program.endtime > :BROWSETS2 ";
    };

    ProgramList progList;
    ProgramList dummySched;
    LoadFromProgram(progList, querystr, bindings, dummySched);

    if (progList.empty())
    {
        Infomap["dbstarttime"] = "";
        return;
    }

    const ProgramInfo* prog = (direction == BROWSE_LEFT) ?
        progList[static_cast<uint>(progList.size() - 1)] : progList[0];
    Infomap["dbstarttime"] = prog->GetScheduledStartTime(MythDate::ISODate);
}

void TVBrowseHelper::run()
{
    RunProlog();
    QMutexLocker locker(&m_lock);
    while (true)
    {
        while (m_list.empty() && m_run)
            m_wait.wait(&m_lock);

        if (!m_run)
            break;

        BrowseInfo bi = m_list.front();
        m_list.pop_front();

        vector<uint> chanids;
        if (BROWSE_SAME == bi.m_dir)
        {
            if (!bi.m_chanId)
            {
                vector<uint> chanids_extra;
                uint sourceid = m_dbChanidToSourceid[m_chanId];
                QMultiMap<QString,uint>::iterator it;
                it = m_dbChannumToChanids.lowerBound(bi.m_chanNum);
                for ( ; (it != m_dbChannumToChanids.end()) &&
                          (it.key() == bi.m_chanNum); ++it)
                {
                    if (m_dbChanidToSourceid[*it] == sourceid)
                        chanids.push_back(*it);
                    else
                        chanids_extra.push_back(*it);
                }
                chanids.insert(chanids.end(),
                               chanids_extra.begin(),
                               chanids_extra.end());
            }
            m_chanNum   = bi.m_chanNum;
            m_chanId    = (chanids.empty()) ? bi.m_chanId : chanids[0];
            m_startTime = bi.m_startTime;
        }

        BrowseDirection direction = bi.m_dir;

        QDateTime lasttime = MythDate::fromString(m_startTime);
        QDateTime curtime  = MythDate::current();
        if (lasttime < curtime)
            m_startTime = curtime.toString(Qt::ISODate);

        QDateTime maxtime  = curtime.addSecs(m_dbBrowseMaxForward);
        if ((lasttime > maxtime) && (direction == BROWSE_RIGHT))
            continue;

        m_lock.unlock();

        // if browsing channel groups is enabled or
        // direction if BROWSE_FAVORITES
        // Then pick the next channel in the channel group list to browse
        // If channel group is ALL CHANNELS (-1), then bypass picking from
        // the channel group list
        if ((m_dbUseChannelGroups || (direction == BROWSE_FAVORITE)) &&
            (direction != BROWSE_RIGHT) && (direction != BROWSE_LEFT) &&
            (direction != BROWSE_SAME))
        {
            m_tv->m_channelGroupLock.lock();
            if (m_tv->m_channelGroupId > -1)
            {
                ChannelChangeDirection dir = CHANNEL_DIRECTION_SAME;
                if ((direction == BROWSE_UP) || (direction == BROWSE_FAVORITE))
                    dir = CHANNEL_DIRECTION_UP;
                else if (direction == BROWSE_DOWN)
                    dir = CHANNEL_DIRECTION_DOWN;

                uint chanid = ChannelUtil::GetNextChannel(
                    m_tv->m_channelGroupChannelList, m_chanId, 0, 0, dir);
                direction = BROWSE_SAME;

                m_tv->m_channelGroupLock.unlock();

                m_lock.lock();
                m_chanId  = chanid;
                m_chanNum.clear();
                m_lock.unlock();
            }
            else
            {
                m_tv->m_channelGroupLock.unlock();
            }
        }

        if (direction == BROWSE_FAVORITE)
            direction = BROWSE_UP;

        InfoMap infoMap;
        infoMap["dbstarttime"] = m_startTime;
        infoMap["channum"]     = m_chanNum;
        infoMap["chanid"]      = QString::number(m_chanId);

        m_tv->GetPlayerReadLock();

        if (!m_dbBrowseAllTuners)
        {
            GetNextProgram(direction, infoMap);
        }
        else
        {
            if (!chanids.empty())
            {
                for (uint chanid : chanids)
                {
                    if (TV::IsTunable(chanid))
                    {
                        infoMap["chanid"] = QString::number(chanid);
                        GetNextProgramDB(direction, infoMap);
                        break;
                    }
                }
            }
            else
            {
                uint orig_chanid = infoMap["chanid"].toUInt();
                GetNextProgramDB(direction, infoMap);
                while (!TV::IsTunable(infoMap["chanid"].toUInt()) &&
                       (infoMap["chanid"].toUInt() != orig_chanid))
                {
                    GetNextProgramDB(direction, infoMap);
                }
            }
        }
        m_tv->ReturnPlayerLock();

        m_lock.lock();
        if (!m_ctx)
            continue;

        m_chanNum = infoMap["channum"];
        m_chanId  = infoMap["chanid"].toUInt();

        if (((direction == BROWSE_LEFT) || (direction == BROWSE_RIGHT)) &&
            !infoMap["dbstarttime"].isEmpty())
        {
            m_startTime = infoMap["dbstarttime"];
        }

        if (!m_list.empty())
        {
            // send partial info to UI for appearance of responsiveness
            QCoreApplication::postEvent(m_tv, new UpdateBrowseInfoEvent(infoMap));
            continue;
        }
        m_lock.unlock();

        // pull in additional data from the DB...
        if (m_tv->m_channelGroupId > -1 && m_dbUseChannelGroups)
            infoMap["channelgroup"] = ChannelGroup::GetChannelGroupName(m_tv->m_channelGroupId);
        else
            infoMap["channelgroup"] = QObject::tr("All channels");

        QDateTime startts = MythDate::fromString(m_startTime);
        RecordingInfo recinfo(m_chanId, startts, false);
        recinfo.ToMap(infoMap);
        infoMap["iconpath"] = ChannelUtil::GetIcon(recinfo.GetChanID());

        m_lock.lock();
        if (m_ctx)
            QCoreApplication::postEvent(m_tv, new UpdateBrowseInfoEvent(infoMap));
    }
    RunEpilog();
}
