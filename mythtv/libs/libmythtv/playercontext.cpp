#include <cmath>
#include <utility>

#include <QPainter>

#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/storagegroup.h"

#include "Bluray/mythbdplayer.h"
#include "DVD/mythdvdplayer.h"
#include "channelutil.h"
#include "io/mythmediabuffer.h"
#include "livetvchain.h"
#include "metadataimagehelper.h"
#include "mythplayer.h"
#include "playercontext.h"
#include "playgroup.h"
#include "remoteencoder.h"
#include "tv_play.h"
#include "videometadatautil.h"
#include "videoouttypes.h"

#define LOC QString("playCtx: ")

PlayerContext::PlayerContext(QString inUseID) :
    m_recUsage(std::move(inUseID))
{
    m_lastSignalMsgTime.start();
    m_lastSignalMsgTime.addMSecs(-2 * kSMExitTimeout);
}

PlayerContext::~PlayerContext()
{
    TeardownPlayer();
    m_nextState.clear();
}

void PlayerContext::TeardownPlayer(void)
{
    m_ffRewState = 0;
    m_ffRewIndex = 0;
    m_ffRewSpeed = 0;
    m_tsNormal   = 1.0F;

    SetPlayer(nullptr);
    SetRecorder(nullptr);
    SetRingBuffer(nullptr);
    SetTVChain(nullptr);
    SetPlayingInfo(nullptr);
}

/**
 * \brief determine initial tv state and playgroup for the recording
 * \param islivetv: true if recording is livetv
 */
void PlayerContext::SetInitialTVState(bool islivetv)
{
    TVState newState = kState_None;
    QString newPlaygroup("Default");

    LockPlayingInfo(__FILE__, __LINE__);
    if (islivetv)
    {
        SetTVChain(new LiveTVChain());
        newState = kState_WatchingLiveTV;
    }
    else if (m_playingInfo)
    {
        int overrecordseconds = gCoreContext->GetNumSetting("RecordOverTime");
        QDateTime curtime = MythDate::current();
        QDateTime recendts = m_playingInfo->GetRecordingEndTime()
            .addSecs(overrecordseconds);

        if (m_playingInfo->IsRecording())
        {
            newState = (curtime < recendts) ?
                kState_WatchingRecording : kState_WatchingPreRecorded;
        }
        else if (m_playingInfo->IsVideoDVD())
        {
            newState = kState_WatchingDVD;
        }
        else if (m_playingInfo->IsVideoBD())
        {
            newState = kState_WatchingBD;
        }
        else
        {
            newState = kState_WatchingVideo;
        }

        newPlaygroup = m_playingInfo->GetPlaybackGroup();
    }
    UnlockPlayingInfo(__FILE__, __LINE__);

    ChangeState(newState);
    SetPlayGroup(newPlaygroup);
}

bool PlayerContext::HasPlayer(void) const
{
    QMutexLocker locker(&m_deletePlayerLock);
    return m_player;
}

bool PlayerContext::IsPlayerErrored(void) const
{
    QMutexLocker locker(&m_deletePlayerLock);
    return m_player && m_player->IsErrored();
}

bool PlayerContext::IsPlayerPlaying(void) const
{
    QMutexLocker locker(&m_deletePlayerLock);
    return m_player && m_player->IsPlaying();
}

bool PlayerContext::HandlePlayerSpeedChangeFFRew(void)
{
    QMutexLocker locker(&m_deletePlayerLock);
    if ((m_ffRewState || m_ffRewSpeed) && m_player && m_player->AtNormalSpeed())
    {
        m_ffRewSpeed = 0;
        m_ffRewState = 0;
        m_ffRewIndex = TV::kInitFFRWSpeed;
        return true;
    }
    return false;
}

bool PlayerContext::HandlePlayerSpeedChangeEOF(void)
{
    QMutexLocker locker(&m_deletePlayerLock);
    if (m_player && (m_player->GetNextPlaySpeed() != m_tsNormal) &&
        m_player->AtNormalSpeed())
    {
        // Speed got changed in player since we are close to the end of file
        m_tsNormal = 1.0F;
        return true;
    }
    return false;
}

bool PlayerContext::IsRecorderErrored(void) const
{
    return m_recorder && m_recorder->GetErrorStatus();
}

void PlayerContext::StopPlaying(void) const
{
    if (m_player)
        m_player->StopPlaying();
}

void PlayerContext::UpdateTVChain(const QStringList &data)
{
    QMutexLocker locker(&m_deletePlayerLock);
    if (m_tvchain && m_player)
    {
        m_tvchain->ReloadAll(data);
        m_player->CheckTVChain();
    }
}

bool PlayerContext::ReloadTVChain(void)
{
    if (!m_tvchain)
        return false;

    m_tvchain->ReloadAll();
    ProgramInfo *pinfo = m_tvchain->GetProgramAt(-1);
    if (pinfo)
    {
        SetPlayingInfo(pinfo);
        delete pinfo;
        return true;
    }
    return false;
}

/**
 * \brief most recently selected channel to the previous channel list
 */
void PlayerContext::PushPreviousChannel(void)
{
    if (!m_tvchain)
        return;

    // Don't store more than kMaxChannelHistory channels. Remove the first item
    if (m_prevChan.size() >= kMaxChannelHistory)
        m_prevChan.pop_front();

    // This method builds the stack of previous channels
    QString curChan = m_tvchain->GetChannelName(-1);
    if (m_prevChan.empty() ||
        curChan != m_prevChan[m_prevChan.size() - 1])
    {
        const QString& chan = curChan;
        m_prevChan.push_back(chan);
    }
}

QString PlayerContext::PopPreviousChannel(void)
{
    if (m_prevChan.empty())
        return {};

    QString curChan = m_tvchain->GetChannelName(-1);
    if ((curChan == m_prevChan.back()) && !m_prevChan.empty())
        m_prevChan.pop_back();

    if (m_prevChan.empty())
        return {};

    QString chan = m_prevChan.back();
    m_prevChan.pop_back();
    // add the current channel back to the list, to allow easy flipping between
    // two channels using PREVCHAN
    PushPreviousChannel();
    return chan;
}

QString PlayerContext::GetPreviousChannel(void) const
{
    if (m_prevChan.empty())
        return {};

    QString curChan = m_tvchain->GetChannelName(-1);
    QString preChan;
    if (curChan != m_prevChan.back() || m_prevChan.size() < 2)
        preChan = m_prevChan.back();
    else
        preChan = m_prevChan[m_prevChan.size()-2];
    return preChan;
}

void PlayerContext::LockPlayingInfo([[maybe_unused]] const char *file,
                                    [[maybe_unused]] int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("LockPlayingInfo(%1,%2)")
            .arg(file).arg(line));
#endif
    m_playingInfoLock.lock();
}

void PlayerContext::UnlockPlayingInfo([[maybe_unused]] const char *file,
                                      [[maybe_unused]] int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("UnlockPlayingInfo(%1,%2)")
            .arg(file).arg(line));
#endif
    m_playingInfoLock.unlock();
}

/**
 * \brief prevent MythPlayer from being deleted
 *        used to ensure player can only be deleted after
 *        osd in TV() is unlocked.
 */
void PlayerContext::LockDeletePlayer([[maybe_unused]] const char *file,
                                     [[maybe_unused]] int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("LockDeletePlayer(%1,%2)")
            .arg(file).arg(line));
#endif
    m_deletePlayerLock.lock();
}

/**
 * \brief allow player to be deleted.
 */
void PlayerContext::UnlockDeletePlayer([[maybe_unused]] const char *file,
                                       [[maybe_unused]] int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("UnlockDeletePlayer(%1,%2)")
            .arg(file).arg(line));
#endif
    m_deletePlayerLock.unlock();
}

void PlayerContext::LockState(void) const
{
    m_stateLock.lock();
}

void PlayerContext::UnlockState(void) const
{
    m_stateLock.unlock();
}

bool PlayerContext::InStateChange(void) const
{
    if (!m_stateLock.tryLock())
        return true;
    bool inStateChange = !m_nextState.empty();
    m_stateLock.unlock();
    return inStateChange;
}

/**
*   \brief Puts a state change on the nextState queue.
*/
void PlayerContext::ChangeState(TVState newState)
{
    QMutexLocker locker(&m_stateLock);
    m_nextState.enqueue(newState);
}

TVState PlayerContext::DequeueNextState(void)
{
    QMutexLocker locker(&m_stateLock);
    return m_nextState.dequeue();
}

/**
 * \brief Removes any pending state changes, and puts kState_None on the queue.
 */
void PlayerContext::ForceNextStateNone(void)
{
    QMutexLocker locker(&m_stateLock);
    m_nextState.clear();
    m_nextState.push_back(kState_None);
}

TVState PlayerContext::GetState(void) const
{
    QMutexLocker locker(&m_stateLock);
    return m_playingState;
}

bool PlayerContext::GetPlayingInfoMap(InfoMap &infoMap) const
{
    bool loaded = false;
    LockPlayingInfo(__FILE__, __LINE__);
    if (m_playingInfo)
    {
        m_playingInfo->ToMap(infoMap);
        infoMap["tvstate"]  = StateToString(m_playingState);
        infoMap["iconpath"] = ChannelUtil::GetIcon(m_playingInfo->GetChanID());
        if ((m_playingInfo->IsVideoFile() || m_playingInfo->IsVideoDVD() ||
            m_playingInfo->IsVideoBD()) && m_playingInfo->GetPathname() !=
            m_playingInfo->GetBasename())
        {
            infoMap["coverartpath"] = VideoMetaDataUtil::GetArtPath(
                m_playingInfo->GetPathname(), "Coverart");
            infoMap["fanartpath"] = VideoMetaDataUtil::GetArtPath(
                m_playingInfo->GetPathname(), "Fanart");
            infoMap["bannerpath"] = VideoMetaDataUtil::GetArtPath(
                m_playingInfo->GetPathname(), "Banners");
            infoMap["screenshotpath"] = VideoMetaDataUtil::GetArtPath(
                m_playingInfo->GetPathname(), "Screenshots");
        }
        else
        {
            ArtworkMap artmap = GetArtwork(m_playingInfo->GetInetRef(),
                                           m_playingInfo->GetSeason());
            infoMap["coverartpath"] =
                artmap.value(kArtworkCoverart).url;
            infoMap["fanartpath"] =
                artmap.value(kArtworkFanart).url;
            infoMap["bannerpath"] =
                artmap.value(kArtworkBanner).url;
            infoMap["screenshotpath"] =
                artmap.value(kArtworkScreenshot).url;
        }
        loaded = true;
    }
    UnlockPlayingInfo(__FILE__, __LINE__);
    return loaded;
}

bool PlayerContext::IsSameProgram(const ProgramInfo &p) const
{
    bool ret = false;
    LockPlayingInfo(__FILE__, __LINE__);
    if (m_playingInfo)
        ret = m_playingInfo->IsSameProgram(p);
    UnlockPlayingInfo(__FILE__, __LINE__);
    return ret;
}

QString PlayerContext::GetFilters(const QString &baseFilters) const
{
    QString filters     = baseFilters;
    QString chanFilters;

    if (gCoreContext->IsDatabaseIgnored())
        return baseFilters;

    LockPlayingInfo(__FILE__, __LINE__);
    if (m_playingInfo) // Recordings have this info already.
        chanFilters = m_playingInfo->GetChannelPlaybackFilters();
    UnlockPlayingInfo(__FILE__, __LINE__);

    if (!chanFilters.isEmpty())
    {
        if ((chanFilters[0] != '+'))
        {
            filters = chanFilters;
        }
        else
        {
            if (!filters.isEmpty() && (!filters.endsWith(",")))
                filters += ",";

            filters += chanFilters.mid(1);
        }
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Output filters for this channel are: '%1'")
                    .arg(filters));

    return filters;
}

QString PlayerContext::GetPlayMessage(void) const
{
    QString mesg = QObject::tr("Play");
    if (m_ffRewState < 0)
    {
        mesg = QObject::tr("Rewind");
        mesg += QString(" %1X").arg(-m_ffRewSpeed);
    }
    else if (m_ffRewState > 0)
    {
        mesg = QObject::tr("Forward");
        mesg += QString(" %1X").arg(m_ffRewSpeed);
    }
    // Make sure these values for m_ffRewSpeed in TV::ChangeSpeed()
    // and PlayerContext::GetPlayMessage() stay in sync.
    else if (m_ffRewSpeed == 0)
    {
        mesg += QString(" %1X").arg(m_tsNormal);
    }
    else if (m_ffRewSpeed == -1)
    {
        mesg += QString(" 1/3X");
    }
    else if (m_ffRewSpeed == -2)
    {
        mesg += QString(" 1/8X");
    }
    else if (m_ffRewSpeed == -3)
    {
        mesg += QString(" 1/16X");
    }

    return mesg;
}

void PlayerContext::SetPlayer(MythPlayer *newplayer)
{
    QMutexLocker locker(&m_deletePlayerLock);
    if (m_player)
    {
        StopPlaying();
        delete m_player;
    }
    m_player = newplayer;
}

void PlayerContext::SetRecorder(RemoteEncoder *rec)
{
    if (m_recorder)
    {
        delete m_recorder;
        m_recorder = nullptr;
    }

    if (rec)
    {
        m_recorder = rec;
        m_lastCardid = m_recorder->GetRecorderNumber();
    }
}

void PlayerContext::SetTVChain(LiveTVChain *chain)
{
    if (m_tvchain)
    {
        m_tvchain->DestroyChain();
        m_tvchain->DecrRef();
        m_tvchain = nullptr;
    }

    m_tvchain = chain;

    if (m_tvchain)
        m_tvchain->InitializeNewChain(gCoreContext->GetHostName());
}

void PlayerContext::SetRingBuffer(MythMediaBuffer *Buffer)
{
    if (m_buffer)
    {
        delete m_buffer;
        m_buffer = nullptr;
    }

    m_buffer = Buffer;
}

/**
 * \brief assign programinfo to the context
 */
void PlayerContext::SetPlayingInfo(const ProgramInfo *info)
{
    bool ignoreDB = gCoreContext->IsDatabaseIgnored();

    QMutexLocker locker(&m_playingInfoLock);

    if (m_playingInfo)
    {
        if (!ignoreDB)
            m_playingInfo->MarkAsInUse(false, m_recUsage);
        delete m_playingInfo;
        m_playingInfo = nullptr;
    }

    if (info)
    {
        m_playingInfo = new ProgramInfo(*info);
        if (!ignoreDB)
            m_playingInfo->MarkAsInUse(true, m_recUsage);
        m_playingLen = m_playingInfo->GetSecondsInRecording();
        m_playingRecStart = m_playingInfo->GetRecordingStartTime();
    }
}

void PlayerContext::SetPlayGroup(const QString &group)
{
    m_fftime       = PlayGroup::GetDurSetting<std::chrono::seconds>(group, "skipahead", 30s);
    m_rewtime      = PlayGroup::GetDurSetting<std::chrono::seconds>(group, "skipback", 5s);
    m_jumptime     = PlayGroup::GetDurSetting<std::chrono::minutes>(group, "jump", 10min);
    m_tsNormal     = PlayGroup::GetSetting(group, "timestretch", 100) * 0.01F;
    m_tsAlt        = (m_tsNormal == 1.0F) ? 1.5F : 1.0F;
}

void PlayerContext::SetPseudoLiveTV(
    const ProgramInfo *pi, PseudoState new_state)
{
    ProgramInfo *old_rec = m_pseudoLiveTVRec;
    ProgramInfo *new_rec = nullptr;

    if (pi)
    {
        new_rec = new ProgramInfo(*pi);
        QString msg = QString("Wants to record: %1 %2 %3 %4")
            .arg(new_rec->GetTitle(), new_rec->GetChanNum(),
                 new_rec->GetRecordingStartTime(MythDate::ISODate),
                 new_rec->GetRecordingEndTime(MythDate::ISODate));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + msg);
    }

    m_pseudoLiveTVRec   = new_rec;
    m_pseudoLiveTVState = new_state;

    if (old_rec)
    {
        QString msg = QString("Done recording: %1 %2 %3 %4")
            .arg(old_rec->GetTitle(), old_rec->GetChanNum(),
                 old_rec->GetRecordingStartTime(MythDate::ISODate),
                 old_rec->GetRecordingEndTime(MythDate::ISODate));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + msg);
        delete old_rec;
    }
}
