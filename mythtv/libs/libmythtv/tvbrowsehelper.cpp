// Qt
#include <QCoreApplication>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "cardutil.h"
#include "channelutil.h"
#include "playercontext.h"
#include "recordinginfo.h"
#include "remoteencoder.h"
#include "tv_play.h"
#include "tvbrowsehelper.h"

#define LOC QString("BrowseHelper: ")

static void format_time(int seconds, QString& tMin, QString& tHrsMin)
{
    int minutes     = seconds / 60;
    int hours       = minutes / 60;
    int min         = minutes % 60;

    tMin = TV::tr("%n minute(s)", "", minutes);
    tHrsMin = QString("%1:%2").arg(hours).arg(min, 2, 10, QChar('0'));
}

TVBrowseHelper::TVBrowseHelper(TV* Parent)
  : MThread("TVBrowseHelper"),
    m_parent(Parent)
{
}

TVBrowseHelper::~TVBrowseHelper()
{
    BrowseStop();
    BrowseWait();
}

void TVBrowseHelper::BrowseInit(std::chrono::seconds BrowseMaxForward, bool BrowseAllTuners,
                                bool UseChannelGroups, const QString &DBChannelOrdering)
{
    m_dbBrowseMaxForward = BrowseMaxForward;
    m_dbBrowseAllTuners  = BrowseAllTuners;
    m_dbUseChannelGroups = UseChannelGroups;

    m_dbAllChannels = ChannelUtil::GetChannels(0, true, "channum, callsign");
    ChannelUtil::SortChannels(m_dbAllChannels, DBChannelOrdering, false);

    for (const auto & chan : m_dbAllChannels)
    {
        m_dbChanidToChannum[chan.m_chanId] = chan.m_chanNum;
        m_dbChanidToSourceid[chan.m_chanId] = chan.m_sourceId;
        m_dbChannumToChanids.insert(chan.m_chanNum,chan.m_chanId);
    }

    start();
}

void TVBrowseHelper::BrowseStop()
{
    QMutexLocker locker(&m_browseLock);
    m_browseList.clear();
    m_browseRun = false;
    m_browseWait.wakeAll();
}

void TVBrowseHelper::BrowseWait()
{
    MThread::wait();
}

/// \brief Begins channel browsing.
/// \note This may only be called from the UI thread.
bool TVBrowseHelper::BrowseStart(bool SkipBrowse)
{
    if (!gCoreContext->IsUIThread())
        return false;

    QMutexLocker locker(&m_browseLock);

    if (m_browseTimerId)
        return true;

    m_parent->ClearOSD();
    m_parent->GetPlayerReadLock();
    PlayerContext* context = m_parent->GetPlayerContext();
    context->LockPlayingInfo(__FILE__, __LINE__);
    if (context->m_playingInfo)
    {
        m_browseChanNum   = context->m_playingInfo->GetChanNum();
        m_browseChanId    = context->m_playingInfo->GetChanID();
        m_browseStartTime = context->m_playingInfo->GetScheduledStartTime(MythDate::ISODate);
        context->UnlockPlayingInfo(__FILE__, __LINE__);
        m_parent->ReturnPlayerLock();

        if (!SkipBrowse)
        {
            BrowseInfo bi(BROWSE_SAME, m_browseChanNum, m_browseChanId, m_browseStartTime);
            locker.unlock();
            BrowseDispInfo(bi);
        }
        return true;
    }
    context->UnlockPlayingInfo(__FILE__, __LINE__);
    m_parent->ReturnPlayerLock();
    return false;
}

/** \brief Ends channel browsing.
 *         Changing the channel if change_channel is true.
 *  \note This may only be called from the UI thread.
 *  \param change_channel iff true we call ChangeChannel()
 */
void TVBrowseHelper::BrowseEnd(bool ChangeChannel)
{
    if (!gCoreContext->IsUIThread())
        return;

    QMutexLocker locker(&m_browseLock);

    if (m_browseTimerId)
    {
        m_parent->KillTimer(m_browseTimerId);
        m_browseTimerId = 0;
    }

    m_browseList.clear();
    m_browseWait.wakeAll();

    OSD* osd = m_parent->GetOSDL();
    if (osd)
        osd->HideWindow(OSD_WIN_BROWSE);
    m_parent->ReturnOSDLock();

    if (ChangeChannel)
        m_parent->ChangeChannel(0, m_browseChanNum);
}

void TVBrowseHelper::BrowseDispInfo(const BrowseInfo& Browseinfo)
{
    if (!gCoreContext->IsUIThread())
        return;

    if (!BrowseStart(true))
        return;

    m_parent->KillTimer(m_browseTimerId);
    m_browseTimerId = m_parent->StartTimer(TV::kBrowseTimeout, __LINE__);

    QMutexLocker locker(&m_browseLock);
    if (BROWSE_SAME == Browseinfo.m_dir)
        m_browseList.clear();
    m_browseList.push_back(Browseinfo);
    m_browseWait.wakeAll();
}

void TVBrowseHelper::BrowseDispInfo(BrowseDirection Direction)
{
    BrowseInfo bi(Direction);
    if (BROWSE_SAME != Direction)
        BrowseDispInfo(bi);
}

void TVBrowseHelper::BrowseChannel(const QString& Channum)
{
    if (!gCoreContext->IsUIThread())
        return;

    if (m_dbBrowseAllTuners)
    {
        BrowseInfo bi(Channum, 0);
        BrowseDispInfo(bi);
        return;
    }

    m_parent->GetPlayerReadLock();
    PlayerContext* context = m_parent->GetPlayerContext();
    if (!context->m_recorder || !context->m_lastCardid)
    {
        m_parent->ReturnPlayerLock();
        return;
    }

    uint inputid  = static_cast<uint>(context->m_lastCardid);
    m_parent->ReturnPlayerLock();
    uint sourceid = CardUtil::GetSourceID(inputid);
    if (sourceid)
    {
        BrowseInfo bi(Channum, sourceid);
        BrowseDispInfo(bi);
    }
}

BrowseInfo TVBrowseHelper::GetBrowsedInfo() const
{
    QMutexLocker locker(&m_browseLock);
    BrowseInfo bi(BROWSE_SAME);
    bi.m_chanNum   = m_browseChanNum;
    bi.m_chanId    = m_browseChanId;
    bi.m_startTime = m_browseStartTime;
    return bi;
}

/** \brief Returns a chanid for the channum, or 0 if none is available.
 *
 *  This will prefer a given sourceid first, and then a given card id,
 *  but if one or the other can not be satisfied but m_dbBrowseAllTuners
 *  is set then it will look to see if the chanid is available on any
 *  tuner.
 */
uint TVBrowseHelper::GetBrowseChanId(const QString& Channum, uint PrefCardid, uint PrefSourceid) const
{
    if (PrefSourceid)
    {
        auto samesourceid = [&Channum, &PrefSourceid](const ChannelInfo& Chan)
            { return Chan.m_sourceId == PrefSourceid && Chan.m_chanNum == Channum; };
        auto chan = std::find_if(m_dbAllChannels.cbegin(), m_dbAllChannels.cend(), samesourceid);
        if (chan != m_dbAllChannels.cend())
            return chan->m_chanId;
    }

    if (PrefCardid)
    {
        auto prefcardid = [&Channum, &PrefCardid](const ChannelInfo& Chan)
            { return Chan.GetInputIds().contains(PrefCardid) && Chan.m_chanNum == Channum; };
        auto chan = std::find_if(m_dbAllChannels.cbegin(), m_dbAllChannels.cend(), prefcardid);
        if (chan != m_dbAllChannels.cend())
            return chan->m_chanId;
    }

    if (m_dbBrowseAllTuners)
    {
        auto channelmatch = [&Channum](const ChannelInfo& Chan) { return Chan.m_chanNum == Channum; };
        auto chan = std::find_if(m_dbAllChannels.cbegin(), m_dbAllChannels.cend(), channelmatch);
        if (chan != m_dbAllChannels.cend())
            return chan->m_chanId;
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
    m_parent->GetPlayerReadLock();
    PlayerContext* context = m_parent->GetPlayerContext();
    if (!context->m_recorder)
    {
        m_parent->ReturnPlayerLock();
        return;
    }

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

    context->m_recorder->GetNextProgram(Direction, title, subtitle, desc, category,
                                      starttime, endtime, callsign, iconpath,
                                      channum, chanid, seriesid, programid);
    m_parent->ReturnPlayerLock();

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
        chanid = ChannelUtil::GetNextChannel(m_dbAllChannels,
                                            chanid,
                                             0 /* mplexid_restriction */,
                                             0 /* chanid restriction */,
                                             static_cast<ChannelChangeDirection>(chandir),
                                             true /* skip non visible */,
                                             true /* skip_same_channum_and_callsign */,
                                             true /* skip_other_sources */);
    }

    Infomap["chanid"]  = QString::number(chanid);
    Infomap["channum"] = m_dbChanidToChannum[chanid];

    QDateTime nowtime = MythDate::current();
    static constexpr int64_t kSixHours {6LL * 60 * 60};
    QDateTime latesttime = nowtime.addSecs(kSixHours);
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
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Helper thread starting");
    QMutexLocker locker(&m_browseLock);
    while (true)
    {
        while (m_browseList.empty() && m_browseRun)
            m_browseWait.wait(&m_browseLock);

        if (!m_browseRun)
            break;

        BrowseInfo bi = m_browseList.front();
        m_browseList.pop_front();

        std::vector<uint> chanids;
        if (BROWSE_SAME == bi.m_dir)
        {
            if (!bi.m_chanId)
            {
                std::vector<uint> chanids_extra;
                uint sourceid = m_dbChanidToSourceid[m_browseChanId];
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
            m_browseChanNum   = bi.m_chanNum;
            m_browseChanId    = (chanids.empty()) ? bi.m_chanId : chanids[0];
            m_browseStartTime = bi.m_startTime;
        }

        BrowseDirection direction = bi.m_dir;

        QDateTime lasttime = MythDate::fromString(m_browseStartTime);
        QDateTime curtime  = MythDate::current();
        if (lasttime < curtime)
            m_browseStartTime = curtime.toString(Qt::ISODate);

        QDateTime maxtime  = curtime.addSecs(m_dbBrowseMaxForward.count());
        if ((lasttime > maxtime) && (direction == BROWSE_RIGHT))
            continue;

        m_browseLock.unlock();

        // if browsing channel groups is enabled or
        // direction if BROWSE_FAVORITES
        // Then pick the next channel in the channel group list to browse
        // If channel group is ALL CHANNELS (-1), then bypass picking from
        // the channel group list
        if ((m_dbUseChannelGroups || (direction == BROWSE_FAVORITE)) &&
            (direction != BROWSE_RIGHT) && (direction != BROWSE_LEFT) &&
            (direction != BROWSE_SAME))
        {
            m_parent->m_channelGroupLock.lock();
            if (m_parent->m_channelGroupId > -1)
            {
                ChannelChangeDirection dir = CHANNEL_DIRECTION_SAME;
                if ((direction == BROWSE_UP) || (direction == BROWSE_FAVORITE))
                    dir = CHANNEL_DIRECTION_UP;
                else if (direction == BROWSE_DOWN)
                    dir = CHANNEL_DIRECTION_DOWN;

                uint chanid = ChannelUtil::GetNextChannel(m_parent->m_channelGroupChannelList, m_browseChanId, 0, 0, dir);
                direction = BROWSE_SAME;

                m_parent->m_channelGroupLock.unlock();

                m_browseLock.lock();
                m_browseChanId  = chanid;
                m_browseChanNum.clear();
                m_browseLock.unlock();
            }
            else
            {
                m_parent->m_channelGroupLock.unlock();
            }
        }

        if (direction == BROWSE_FAVORITE)
            direction = BROWSE_UP;

        InfoMap infoMap;
        infoMap["dbstarttime"] = m_browseStartTime;
        infoMap["channum"]     = m_browseChanNum;
        infoMap["chanid"]      = QString::number(m_browseChanId);

        m_parent->GetPlayerReadLock();

        if (!m_dbBrowseAllTuners)
        {
            GetNextProgram(direction, infoMap);
        }
        else
        {
            if (!chanids.empty())
            {
                auto tunable = [](uint chanid) { return TV::IsTunable(chanid); };
                auto it = std::find_if(chanids.cbegin(), chanids.cend(), tunable);
                if (it != chanids.cend())
                {
                    infoMap["chanid"] = QString::number(*it);
                    GetNextProgramDB(direction, infoMap);
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
        m_parent->ReturnPlayerLock();

        m_browseLock.lock();

        m_browseChanNum = infoMap["channum"];
        m_browseChanId  = infoMap["chanid"].toUInt();

        if (((direction == BROWSE_LEFT) || (direction == BROWSE_RIGHT)) &&
            !infoMap["dbstarttime"].isEmpty())
        {
            m_browseStartTime = infoMap["dbstarttime"];
        }

        if (!m_browseList.empty())
        {
            // send partial info to UI for appearance of responsiveness
            QCoreApplication::postEvent(m_parent, new UpdateBrowseInfoEvent(infoMap));
            continue;
        }
        m_browseLock.unlock();

        // pull in additional data from the DB...
        if (m_parent->m_channelGroupId > -1 && m_dbUseChannelGroups)
            infoMap["channelgroup"] = ChannelGroup::GetChannelGroupName(m_parent->m_channelGroupId);
        else
            infoMap["channelgroup"] = QObject::tr("All channels");

        QDateTime startts = MythDate::fromString(m_browseStartTime);
        RecordingInfo recinfo(m_browseChanId, startts, false);
        recinfo.ToMap(infoMap);
        infoMap["iconpath"] = ChannelUtil::GetIcon(recinfo.GetChanID());

        m_browseLock.lock();
        QCoreApplication::postEvent(m_parent, new UpdateBrowseInfoEvent(infoMap));
    }
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Helper thread exiting");
    RunEpilog();
}
