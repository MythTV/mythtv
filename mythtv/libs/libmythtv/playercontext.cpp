#include <cmath>
#include <utility>

#include <QPainter>

#include "mythconfig.h"

#include "playercontext.h"
#include "mythplayer.h"
#include "remoteencoder.h"
#include "livetvchain.h"
#include "ringbuffer.h"
#include "playgroup.h"
#include "videoouttypes.h"
#include "storagegroup.h"
#include "mythcorecontext.h"
#include "videometadatautil.h"
#include "metadataimagehelper.h"
#include "mythlogging.h"
#include "DVD/mythdvdplayer.h"
#include "Bluray/mythbdplayer.h"
#include "channelutil.h"
#include "tv_play.h"

#define LOC QString("playCtx: ")

const uint PlayerContext::kSMExitTimeout     = 2000;
const uint PlayerContext::kMaxChannelHistory = 30;

PlayerContext::PlayerContext(QString inUseID) :
    m_recUsage(std::move(inUseID))
{
    m_lastSignalMsgTime.start();
    m_lastSignalMsgTime.addMSecs(-2 * (int)kSMExitTimeout);
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
            newState = kState_WatchingDVD;
        else if (m_playingInfo->IsVideoBD())
            newState = kState_WatchingBD;
        else
            newState = kState_WatchingVideo;

        newPlaygroup = m_playingInfo->GetPlaybackGroup();
    }
    UnlockPlayingInfo(__FILE__, __LINE__);

    ChangeState(newState);
    SetPlayGroup(newPlaygroup);
}

/**
 * \brief Check if PIP is supported for current video
 * renderer running. Current support written for XV, Opengl and VDPAU.
 * Not sure about ivtv.
 */
bool PlayerContext::IsPIPSupported(void) const
{
    bool supported = false;
    QMutexLocker locker(&m_deletePlayerLock);
    if (m_player)
    {
        const MythVideoOutput *vid = m_player->GetVideoOutput();
        if (vid)
            supported = vid->IsPIPSupported();
    }
    return supported;
}

/**
 * \brief Check if PBP is supported for current video
 * renderer running. Current support written for XV and Opengl.
 * Not sure about ivtv.
 */
bool PlayerContext::IsPBPSupported(void) const
{
    bool supported = false;
    QMutexLocker locker(&m_deletePlayerLock);
    if (m_player)
    {
        const MythVideoOutput *vid = m_player->GetVideoOutput();
        if (vid)
            supported = vid->IsPBPSupported();
    }
    return supported;
}

void PlayerContext::CreatePIPWindow(const QRect &rect, int pos,
                    QWidget *widget)
{
    QString name;
    if (pos > -1)
    {
        m_pipLocation = pos;
        name = QString("pip player %1").arg(toString((PIPLocation)pos));
    }
    else
        name = "pip player";

    if (widget)
        m_parentWidget = widget;

    m_pipRect = QRect(rect);
}

/**
 * \brief Get PIP more accurate display size for standalone PIP
 * by factoring the aspect ratio of the video.
 */
QRect PlayerContext::GetStandAlonePIPRect(void)
{
    QRect rect = QRect(0, 0, 0, 0);
    QMutexLocker locker(&m_deletePlayerLock);
    if (m_player)
    {
        rect = m_pipRect;

        float saspect = (float)rect.width() / (float)rect.height();
        float vaspect = m_player->GetVideoAspect();

        // Calculate new height or width according to relative aspect ratio
        if (lroundf(saspect * 10) > lroundf(vaspect * 10))
        {
            rect.setWidth((int) ceil(rect.width() * (vaspect / saspect)));
        }
        else if (lroundf(saspect * 10) < lroundf(vaspect * 10))
        {
            rect.setHeight((int) ceil(rect.height() * (saspect / vaspect)));
        }

        rect.setHeight(((rect.height() + 7) / 8) * 8);
        rect.setWidth( ((rect.width()  + 7) / 8) * 8);
    }
    return rect;
}

bool PlayerContext::StartPIPPlayer(TV *tv, TVState desiredState)
{
    bool ok = false;

    if (!m_useNullVideo && m_parentWidget)
    {
        const QRect rect = m_pipRect;
        ok = CreatePlayer(tv, m_parentWidget, desiredState,
                          true, rect);
    }

    if (m_useNullVideo || !ok)
    {
        SetPlayer(nullptr);
        m_useNullVideo = true;
        ok = CreatePlayer(tv, nullptr, desiredState,
                          false);
    }

    return ok;
}


/**
 * \brief stop player but pause the ringbuffer. used in PIP/PBP swap or
 * switching from PIP <-> PBP or enabling PBP
 */

void PlayerContext::PIPTeardown(void)
{
    if (m_buffer)
    {
        m_buffer->Pause();
        m_buffer->WaitForPause();
    }

    {
        QMutexLocker locker(&m_deletePlayerLock);
        StopPlaying();
    }

    SetPlayer(nullptr);

    m_useNullVideo = false;
    m_parentWidget = nullptr;
}

/**
 * \brief Resize PIP Window
 */
void PlayerContext::ResizePIPWindow(const QRect &rect)
{
    if (!IsPIP())
        return;

    QRect tmpRect;
    if (m_pipState == kPIPStandAlone)
        tmpRect = GetStandAlonePIPRect();
    else
        tmpRect = QRect(rect);

    LockDeletePlayer(__FILE__, __LINE__);
    if (m_player && m_player->GetVideoOutput())
        m_player->GetVideoOutput()->ResizeDisplayWindow(tmpRect, false);
    UnlockDeletePlayer(__FILE__, __LINE__);

    m_pipRect = QRect(rect);
}

bool PlayerContext::StartEmbedding(const QRect &embedRect)
{
    bool ret = false;
    LockDeletePlayer(__FILE__, __LINE__);
    if (m_player)
    {
        ret = true;
        m_player->EmbedInWidget(embedRect);
    }
    UnlockDeletePlayer(__FILE__, __LINE__);
    return ret;
}

bool PlayerContext::IsEmbedding(void) const
{
    bool ret = false;
    LockDeletePlayer(__FILE__, __LINE__);
    if (m_player)
        ret = m_player->IsEmbedding();
    UnlockDeletePlayer(__FILE__, __LINE__);
    return ret;
}

void PlayerContext::StopEmbedding(void)
{
    LockDeletePlayer(__FILE__, __LINE__);
    if (m_player)
        m_player->StopEmbedding();
    UnlockDeletePlayer(__FILE__, __LINE__);
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

bool PlayerContext::CalcPlayerSliderPosition(osdInfo &info,
                                             bool paddedFields) const
{
    QMutexLocker locker(&m_deletePlayerLock);
    if (m_player)
    {
        m_player->calcSliderPos(info, paddedFields);
        return true;
    }
    return false;
}

bool PlayerContext::IsRecorderErrored(void) const
{
    return m_recorder && m_recorder->GetErrorStatus();
}

bool PlayerContext::CreatePlayer(TV *tv, QWidget *widget,
                                 TVState desiredState,
                                 bool embed, const QRect &embedbounds,
                                 bool muted)
{
    if (HasPlayer())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "Attempting to setup a player, but it already exists.");
        return false;
    }

    uint playerflags = kDecodeAllowEXT; // allow VDA etc for normal playback
    playerflags |= muted                ? kAudioMuted : kNoFlags;
    playerflags |= m_useNullVideo       ? kVideoIsNull : kNoFlags;
    playerflags |= m_nohardwaredecoders ? kNoFlags : kDecodeAllowGPU;

    MythPlayer *player = nullptr;
    if (kState_WatchingBD  == desiredState)
        player = new MythBDPlayer((PlayerFlags)playerflags);
    else if (kState_WatchingDVD == desiredState)
        player = new MythDVDPlayer((PlayerFlags)playerflags);
    else
        player = new MythPlayer((PlayerFlags)playerflags);

    QString passthru_device =
        gCoreContext->GetBoolSetting("PassThruDeviceOverride", false) ?
        gCoreContext->GetSetting("PassThruOutputDevice") : QString();

    player->SetPlayerInfo(tv, widget, this);
    AudioPlayer *audio = player->GetAudio();
    audio->SetAudioInfo(gCoreContext->GetSetting("AudioOutputDevice"),
                        passthru_device,
                        gCoreContext->GetNumSetting("AudioSampleRate", 44100));
    audio->SetStretchFactor(m_tsNormal);
    player->SetLength(m_playingLen);

    player->AdjustAudioTimecodeOffset(
                0, gCoreContext->GetNumSetting("AudioSyncOffset", 0));

    bool isWatchingRecording = (desiredState == kState_WatchingRecording);
    player->SetWatchingRecording(isWatchingRecording);

    if (!IsAudioNeeded())
        audio->SetNoAudio();
    else
    {
        QString subfn = m_buffer->GetSubtitleFilename();
        bool isInProgress = (desiredState == kState_WatchingRecording ||
                             desiredState == kState_WatchingLiveTV);
        if (!subfn.isEmpty() && player->GetSubReader())
            player->GetSubReader()->LoadExternalSubtitles(subfn, isInProgress);
    }

    if (embed && !embedbounds.isNull())
        player->EmbedInWidget(embedbounds);

    SetPlayer(player);

    if (m_pipState == kPIPOff || m_pipState == kPBPLeft)
    {
        if (IsAudioNeeded())
        {
            // cppcheck-suppress unreadVariable
            QString errMsg = audio->ReinitAudio();
        }
    }
    else if (m_pipState == kPBPRight)
        player->SetMuted(true);

    return StartPlaying(-1);
}

/** \fn PlayerContext::StartPlaying(int)
 *  \brief Starts player, must be called after StartRecorder().
 *  \param maxWait How long to wait for MythPlayer to start playing.
 *  \return true when successful, false otherwise.
 */
bool PlayerContext::StartPlaying(int maxWait)
{
    if (!m_player)
        return false;

    if (!m_player->StartPlaying())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "StartPlaying() Failed to start player");
        // no need to call StopPlaying here as the player context will be deleted
        // later following the error
        return false;
    }
    maxWait = (maxWait <= 0) ? 20000 : maxWait;
#ifdef USING_VALGRIND
    maxWait = (1<<30);
#endif // USING_VALGRIND
    MythTimer t;
    t.start();

    while (!m_player->IsPlaying(50, true) && (t.elapsed() < maxWait))
        ReloadTVChain();

    if (m_player->IsPlaying())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("StartPlaying(): took %1 ms to start player.")
                .arg(t.elapsed()));
        return true;
    }
    LOG(VB_GENERAL, LOG_ERR, LOC + "StartPlaying() Failed to start player");
    StopPlaying();
    return false;
}

void PlayerContext::StopPlaying(void)
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
        return QString();

    QString curChan = m_tvchain->GetChannelName(-1);
    if ((curChan == m_prevChan.back()) && !m_prevChan.empty())
        m_prevChan.pop_back();

    if (m_prevChan.empty())
        return QString();

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
        return QString();

    QString curChan = m_tvchain->GetChannelName(-1);
    QString preChan;
    if (curChan != m_prevChan.back() || m_prevChan.size() < 2)
        preChan = m_prevChan.back();
    else
        preChan = m_prevChan[m_prevChan.size()-2];
    return preChan;
}

void PlayerContext::LockPlayingInfo(const char *file, int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("LockPlayingInfo(%1,%2)")
            .arg(file).arg(line));
#else
    Q_UNUSED(file);
    Q_UNUSED(line);
#endif
    m_playingInfoLock.lock();
}

void PlayerContext::UnlockPlayingInfo(const char *file, int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("UnlockPlayingInfo(%1,%2)")
            .arg(file).arg(line));
#else
    Q_UNUSED(file);
    Q_UNUSED(line);
#endif
    m_playingInfoLock.unlock();
}

/**
 * \brief prevent MythPlayer from being deleted
 *        used to ensure player can only be deleted after
 *        osd in TV() is unlocked.
 */
void PlayerContext::LockDeletePlayer(const char *file, int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("LockDeletePlayer(%1,%2)")
            .arg(file).arg(line));
#else
    Q_UNUSED(file);
    Q_UNUSED(line);
#endif
    m_deletePlayerLock.lock();
}

/**
 * \brief allow player to be deleted.
 */
void PlayerContext::UnlockDeletePlayer(const char *file, int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("UnlockDeletePlayer(%1,%2)")
            .arg(file).arg(line));
#else
    Q_UNUSED(file);
    Q_UNUSED(line);
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

void PlayerContext::LockOSD() const
{
    m_player->LockOSD();
}

void PlayerContext::UnlockOSD(void) const
{
    m_player->UnlockOSD();
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
        if (m_player)
            m_player->GetCodecDescription(infoMap);

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
    if (m_tsNormal != 1.0F)
    {
        if (m_tsNormal == 0.5F)
            mesg += QString(" 1/2x");
        else if (0.32F < m_tsNormal && m_tsNormal < 0.34F)
            mesg += QString(" 1/3x");
        else if (m_tsNormal == 0.25F)
            mesg += QString(" 1/4x");
        else if (m_tsNormal == 0.125F)
            mesg += QString(" 1/8x");
        else if (m_tsNormal == 0.0625F)
            mesg += QString(" 1/16x");
        else
            mesg += QString(" %1x").arg(m_tsNormal);
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
    {
#if 0
        QString seed = QString("");

        if (IsPIP())
            seed = "PIP";

        seed += gCoreContext->GetHostName();
#endif
        m_tvchain->InitializeNewChain(gCoreContext->GetHostName());
    }
}

void PlayerContext::SetRingBuffer(RingBuffer *buf)
{
    if (m_buffer)
    {
        delete m_buffer;
        m_buffer = nullptr;
    }

    m_buffer = buf;
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
    }
}

void PlayerContext::SetPlayGroup(const QString &group)
{
    m_fftime       = PlayGroup::GetSetting(group, "skipahead", 30);
    m_rewtime      = PlayGroup::GetSetting(group, "skipback", 5);
    m_jumptime     = PlayGroup::GetSetting(group, "jump", 10);
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
            .arg(new_rec->GetTitle()).arg(new_rec->GetChanNum())
            .arg(new_rec->GetRecordingStartTime(MythDate::ISODate))
            .arg(new_rec->GetRecordingEndTime(MythDate::ISODate));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + msg);
    }

    m_pseudoLiveTVRec   = new_rec;
    m_pseudoLiveTVState = new_state;

    if (old_rec)
    {
        QString msg = QString("Done recording: %1 %2 %3 %4")
            .arg(old_rec->GetTitle()).arg(old_rec->GetChanNum())
            .arg(old_rec->GetRecordingStartTime(MythDate::ISODate))
            .arg(old_rec->GetRecordingEndTime(MythDate::ISODate));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + msg);
        delete old_rec;
    }
}
