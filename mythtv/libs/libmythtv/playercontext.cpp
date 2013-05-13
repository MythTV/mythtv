#include <cmath>

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

#define LOC QString("playCtx: ")

const uint PlayerContext::kSMExitTimeout     = 2000;
const uint PlayerContext::kMaxChannelHistory = 30;

PlayerContext::PlayerContext(const QString &inUseID) :
    recUsage(inUseID), player(NULL), playerUnsafe(false), recorder(NULL),
    tvchain(NULL), buffer(NULL), playingInfo(NULL), playingLen(0),
    nohardwaredecoders(false), last_cardid(-1),
    // Fast forward state
    ff_rew_state(0), ff_rew_index(0), ff_rew_speed(0),
    // Other state
    playingState(kState_None),
    errored(false),
    // pseudo states
    pseudoLiveTVRec(NULL), pseudoLiveTVState(kPseudoNormalLiveTV),
    // DB values
    fftime(0), rewtime(0),
    jumptime(0), ts_normal(1.0f), ts_alt(1.5f),
    // locks
    playingInfoLock(QMutex::Recursive), deletePlayerLock(QMutex::Recursive),
    stateLock(QMutex::Recursive),
    // pip
    pipState(kPIPOff), pipRect(0,0,0,0), parentWidget(NULL), pipLocation(0),
    useNullVideo(false)
{
    lastSignalMsgTime.start();
    lastSignalMsgTime.addMSecs(-2 * (int)kSMExitTimeout);
}

PlayerContext::~PlayerContext()
{
    TeardownPlayer();
    nextState.clear();
}

void PlayerContext::TeardownPlayer(void)
{
    ff_rew_state = 0;
    ff_rew_index = 0;
    ff_rew_speed = 0;
    ts_normal    = 1.0f;

    SetPlayer(NULL);
    SetRecorder(NULL);
    SetRingBuffer(NULL);
    SetTVChain(NULL);
    SetPlayingInfo(NULL);
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
    else if (playingInfo)
    {
        int overrecordseconds = gCoreContext->GetNumSetting("RecordOverTime");
        QDateTime curtime = MythDate::current();
        QDateTime recendts = playingInfo->GetRecordingEndTime()
            .addSecs(overrecordseconds);

        if (playingInfo->IsRecording())
        {
            newState = (curtime < recendts) ?
                kState_WatchingRecording : kState_WatchingPreRecorded;
        }
        else if (playingInfo->IsVideoDVD())
            newState = kState_WatchingDVD;
        else if (playingInfo->IsVideoBD())
            newState = kState_WatchingBD;
        else
            newState = kState_WatchingVideo;

        newPlaygroup = playingInfo->GetPlaybackGroup();
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
    QMutexLocker locker(&deletePlayerLock);
    if (player)
    {
        const VideoOutput *vid = player->GetVideoOutput();
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
    QMutexLocker locker(&deletePlayerLock);
    if (player)
    {
        const VideoOutput *vid = player->GetVideoOutput();
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
        pipLocation = pos;
        name = QString("pip player %1").arg(toString((PIPLocation)pos));
    }
    else
        name = "pip player";

    if (widget)
        parentWidget = widget;

    pipRect = QRect(rect);
}

/**
 * \brief Get PIP more accurate display size for standalone PIP
 * by factoring the aspect ratio of the video.
 */
QRect PlayerContext::GetStandAlonePIPRect(void)
{
    QRect rect = QRect(0, 0, 0, 0);
    QMutexLocker locker(&deletePlayerLock);
    if (player)
    {
        rect = QRect(pipRect);

        float saspect = (float)rect.width() / (float)rect.height();
        float vaspect = player->GetVideoAspect();

        // Calculate new height or width according to relative aspect ratio
        if ((int)((saspect + 0.05) * 10) > (int)((vaspect + 0.05) * 10))
        {
            rect.setWidth((int) ceil(rect.width() * (vaspect / saspect)));
        }
        else if ((int)((saspect + 0.05) * 10) < (int)((vaspect + 0.05) * 10))
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

    if (!useNullVideo && parentWidget)
    {
        const QRect rect = QRect(pipRect);
        ok = CreatePlayer(tv, parentWidget, desiredState,
                          true, rect);
    }

    if (useNullVideo || !ok)
    {
        SetPlayer(NULL);
        useNullVideo = true;
        ok = CreatePlayer(tv, NULL, desiredState,
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
    if (buffer)
    {
        buffer->Pause();
        buffer->WaitForPause();
    }

    {
        QMutexLocker locker(&deletePlayerLock);
        StopPlaying();
    }

    SetPlayer(NULL);

    useNullVideo = false;
    parentWidget = NULL;
}

/**
 * \brief Resize PIP Window
 */
void PlayerContext::ResizePIPWindow(const QRect &rect)
{
    if (!IsPIP())
        return;

    QRect tmpRect;
    if (pipState == kPIPStandAlone)
        tmpRect = GetStandAlonePIPRect();
    else
        tmpRect = QRect(rect);

    LockDeletePlayer(__FILE__, __LINE__);
    if (player && player->GetVideoOutput())
        player->GetVideoOutput()->ResizeDisplayWindow(tmpRect, false);
    UnlockDeletePlayer(__FILE__, __LINE__);

    pipRect = QRect(rect);
}

bool PlayerContext::StartEmbedding(WId wid, const QRect &embedRect)
{
    bool ret = false;
    LockDeletePlayer(__FILE__, __LINE__);
    if (player)
    {
        ret = true;
        player->EmbedInWidget(embedRect);
    }
    UnlockDeletePlayer(__FILE__, __LINE__);
    return ret;
}

bool PlayerContext::IsEmbedding(void) const
{
    bool ret = false;
    LockDeletePlayer(__FILE__, __LINE__);
    if (player)
        ret = player->IsEmbedding();
    UnlockDeletePlayer(__FILE__, __LINE__);
    return ret;
}

void PlayerContext::StopEmbedding(void)
{
    LockDeletePlayer(__FILE__, __LINE__);
    if (player)
        player->StopEmbedding();
    UnlockDeletePlayer(__FILE__, __LINE__);
}

bool PlayerContext::HasPlayer(void) const
{
    QMutexLocker locker(&deletePlayerLock);
    return player;
}

bool PlayerContext::IsPlayerErrored(void) const
{
    QMutexLocker locker(&deletePlayerLock);
    return player && player->IsErrored();
}

bool PlayerContext::IsPlayerRecoverable(void) const
{
    QMutexLocker locker(&deletePlayerLock);
    return player && player->IsErrorRecoverable();
}

bool PlayerContext::IsPlayerDecoderErrored(void) const
{
    QMutexLocker locker(&deletePlayerLock);
    return player && player->IsDecoderErrored();
}

bool PlayerContext::IsPlayerPlaying(void) const
{
    QMutexLocker locker(&deletePlayerLock);
    return player && player->IsPlaying();
}

bool PlayerContext::HandlePlayerSpeedChangeFFRew(void)
{
    QMutexLocker locker(&deletePlayerLock);
    if ((ff_rew_state || ff_rew_speed) && player && player->AtNormalSpeed())
    {
        ff_rew_speed = 0;
        ff_rew_state = 0;
        ff_rew_index = TV::kInitFFRWSpeed;
        return true;
    }
    return false;
}

bool PlayerContext::HandlePlayerSpeedChangeEOF(void)
{
    QMutexLocker locker(&deletePlayerLock);
    if (player && (player->GetNextPlaySpeed() != ts_normal) &&
        player->AtNormalSpeed())
    {
        // Speed got changed in player since we are close to the end of file
        ts_normal = 1.0f;
        return true;
    }
    return false;
}

bool PlayerContext::CalcPlayerSliderPosition(osdInfo &info,
                                             bool paddedFields) const
{
    QMutexLocker locker(&deletePlayerLock);
    if (player)
    {
        player->calcSliderPos(info);
        return true;
    }
    return false;
}

bool PlayerContext::IsRecorderErrored(void) const
{
    return recorder && recorder->GetErrorStatus();
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
    playerflags |= muted              ? kAudioMuted : kNoFlags;
    playerflags |= useNullVideo       ? kVideoIsNull : kNoFlags;
    playerflags |= nohardwaredecoders ? kNoFlags : kDecodeAllowGPU;

    MythPlayer *player = NULL;
    if (kState_WatchingBD  == desiredState)
        player = new MythBDPlayer((PlayerFlags)playerflags);
    else if (kState_WatchingDVD == desiredState)
        player = new MythDVDPlayer((PlayerFlags)playerflags);
    else
        player = new MythPlayer((PlayerFlags)playerflags);

    QString passthru_device = 
        gCoreContext->GetNumSetting("PassThruDeviceOverride", false) ?
        gCoreContext->GetSetting("PassThruOutputDevice") : QString::null;

    player->SetPlayerInfo(tv, widget, this);
    AudioPlayer *audio = player->GetAudio();
    audio->SetAudioInfo(gCoreContext->GetSetting("AudioOutputDevice"),
                        passthru_device,
                        gCoreContext->GetNumSetting("AudioSampleRate", 44100));
    audio->SetStretchFactor(ts_normal);
    player->SetLength(playingLen);

    player->SetVideoFilters((useNullVideo) ? "onefield" : "");

    if (!IsAudioNeeded())
        audio->SetNoAudio();
    else
    {
        QString subfn = buffer->GetSubtitleFilename();
        if (!subfn.isEmpty() && player->GetSubReader())
            player->GetSubReader()->LoadExternalSubtitles(subfn);
    }

    if (embed && !embedbounds.isNull())
        player->EmbedInWidget(embedbounds);

    bool isWatchingRecording = (desiredState == kState_WatchingRecording);
    player->SetWatchingRecording(isWatchingRecording);

    SetPlayer(player);

    if (pipState == kPIPOff || pipState == kPBPLeft)
    {
        if (audio->HasAudioOut())
        {
            QString errMsg = audio->ReinitAudio();
        }
    }
    else if (pipState == kPBPRight)
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
    if (!player)
        return false;

    player->StartPlaying();

    maxWait = (maxWait <= 0) ? 20000 : maxWait;
#ifdef USING_VALGRIND
    maxWait = (1<<30);
#endif // USING_VALGRIND
    MythTimer t;
    t.start();

    while (!player->IsPlaying(50, true) && (t.elapsed() < maxWait))
        ReloadTVChain();

    if (player->IsPlaying())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("StartPlaying(): took %1 ms to start player.")
                .arg(t.elapsed()));
        return true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "StartPlaying() Failed to start player");
        StopPlaying();
        return false;
    }
}

void PlayerContext::StopPlaying(void)
{
    if (player)
        player->StopPlaying();
}

void PlayerContext::UpdateTVChain(void)
{
    QMutexLocker locker(&deletePlayerLock);
    if (tvchain && player)
    {
        tvchain->ReloadAll();
        player->CheckTVChain();
    }
}

bool PlayerContext::ReloadTVChain(void)
{
    if (!tvchain)
        return false;

    tvchain->ReloadAll();
    ProgramInfo *pinfo = tvchain->GetProgramAt(-1);
    if (pinfo)
    {
        SetPlayingInfo(pinfo);
        delete pinfo;
    }
    return (bool) pinfo;
}

/**
 * \brief most recently selected channel to the previous channel list
 */
void PlayerContext::PushPreviousChannel(void)
{
    if (!tvchain)
        return;

    // Don't store more than kMaxChannelHistory channels. Remove the first item
    if (prevChan.size() >= kMaxChannelHistory)
        prevChan.pop_front();

    // This method builds the stack of previous channels
    QString curChan = tvchain->GetChannelName(-1);
    if (prevChan.empty() ||
        curChan != prevChan[prevChan.size() - 1])
    {
        QString chan = curChan;
        prevChan.push_back(chan);
    }
}

QString PlayerContext::PopPreviousChannel(void)
{
    if (prevChan.empty())
        return QString::null;

    QString curChan = tvchain->GetChannelName(-1);
    if ((curChan == prevChan.back()) && !prevChan.empty())
        prevChan.pop_back();

    if (prevChan.empty())
        return QString::null;

    QString chan = prevChan.back();
    prevChan.pop_back();
    // add the current channel back to the list, to allow easy flipping between
    // two channels using PREVCHAN
    PushPreviousChannel();
    chan.detach();
    return chan;
}

QString PlayerContext::GetPreviousChannel(void) const
{
    if (prevChan.empty())
        return QString::null;

    QString curChan = tvchain->GetChannelName(-1);
    QString preChan = QString::null;
    if (curChan != prevChan.back() || prevChan.size() < 2)
        preChan = prevChan.back();
    else
        preChan = prevChan[prevChan.size()-2];
    preChan.detach();
    return preChan;
}

void PlayerContext::LockPlayingInfo(const char *file, int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("LockPlayingInfo(%1,%2)")
            .arg(file).arg(line));
#endif
    playingInfoLock.lock();
}

void PlayerContext::UnlockPlayingInfo(const char *file, int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("UnlockPlayingInfo(%1,%2)")
            .arg(file).arg(line));
#endif
    playingInfoLock.unlock();
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
#endif
    deletePlayerLock.lock();
}

/**
 * \brief allow player to be deleted.
 */
void PlayerContext::UnlockDeletePlayer(const char *file, int line) const
{
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("UnlockDeletePlayer(%1,%2)")
            .arg(file).arg(line));
#endif
    deletePlayerLock.unlock();
}

void PlayerContext::LockState(void) const
{
    stateLock.lock();
}

void PlayerContext::UnlockState(void) const
{
    stateLock.unlock();
}

void PlayerContext::LockOSD() const
{
    player->LockOSD();
}

void PlayerContext::UnlockOSD(void) const
{
    player->UnlockOSD();
}

bool PlayerContext::InStateChange(void) const
{
    if (!stateLock.tryLock())
        return true;
    bool inStateChange = nextState.size() > 0;
    stateLock.unlock();
    return inStateChange;
}

/**
*   \brief Puts a state change on the nextState queue.
*/
void PlayerContext::ChangeState(TVState newState)
{
    QMutexLocker locker(&stateLock);
    nextState.enqueue(newState);
}

TVState PlayerContext::DequeueNextState(void)
{
    QMutexLocker locker(&stateLock);
    return nextState.dequeue();
}

/**
 * \brief Removes any pending state changes, and puts kState_None on the queue.
 */
void PlayerContext::ForceNextStateNone(void)
{
    QMutexLocker locker(&stateLock);
    nextState.clear();
    nextState.push_back(kState_None);
}

TVState PlayerContext::GetState(void) const
{
    QMutexLocker locker(&stateLock);
    return playingState;
}

bool PlayerContext::GetPlayingInfoMap(InfoMap &infoMap) const
{
    bool loaded = false;
    LockPlayingInfo(__FILE__, __LINE__);
    if (playingInfo)
    {
        playingInfo->ToMap(infoMap);
        infoMap["tvstate"]  = StateToString(playingState);
        infoMap["iconpath"] = ChannelUtil::GetIcon(playingInfo->GetChanID());
        if ((playingInfo->IsVideoFile() || playingInfo->IsVideoDVD() ||
            playingInfo->IsVideoBD()) && playingInfo->GetPathname() !=
            playingInfo->GetBasename())
        {
            infoMap["coverartpath"] = VideoMetaDataUtil::GetArtPath(
                playingInfo->GetPathname(), "Coverart");
            infoMap["fanartpath"] = VideoMetaDataUtil::GetArtPath(
                playingInfo->GetPathname(), "Fanart");
            infoMap["bannerpath"] = VideoMetaDataUtil::GetArtPath(
                playingInfo->GetPathname(), "Banners");
            infoMap["screenshotpath"] = VideoMetaDataUtil::GetArtPath(
                playingInfo->GetPathname(), "Screenshots");
        }
        else
        {
            ArtworkMap artmap = GetArtwork(playingInfo->GetInetRef(),
                                        playingInfo->GetSeason());
            infoMap["coverartpath"] =
                artmap.value(kArtworkCoverart).url;
            infoMap["fanartpath"] =
                artmap.value(kArtworkFanart).url;
            infoMap["bannerpath"] =
                artmap.value(kArtworkBanner).url;
            infoMap["screenshotpath"] =
                artmap.value(kArtworkScreenshot).url;
        }
        if (player)
            player->GetCodecDescription(infoMap);

        infoMap.detach();
        loaded = true;
    }
    UnlockPlayingInfo(__FILE__, __LINE__);
    return loaded;
}

bool PlayerContext::IsSameProgram(const ProgramInfo &p) const
{
    bool ret = false;
    LockPlayingInfo(__FILE__, __LINE__);
    if (playingInfo)
        ret = playingInfo->IsSameProgram(p);
    UnlockPlayingInfo(__FILE__, __LINE__);
    return ret;
}

QString PlayerContext::GetFilters(const QString &baseFilters) const
{
    QString filters     = baseFilters;
    QString chanFilters = QString::null;

    if (gCoreContext->IsDatabaseIgnored())
        return baseFilters;

    LockPlayingInfo(__FILE__, __LINE__);
    if (playingInfo) // Recordings have this info already.
    {
        chanFilters = playingInfo->GetChannelPlaybackFilters();
        chanFilters.detach();
    }
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

    filters.detach();
    return filters;
}

QString PlayerContext::GetPlayMessage(void) const
{
    QString mesg = QObject::tr("Play");
    if (ts_normal != 1.0)
    {
        if (ts_normal == 0.5)
            mesg += QString(" 1/2x");
        else if (0.32 < ts_normal && ts_normal < 0.34)
            mesg += QString(" 1/3x");
        else if (ts_normal == 0.25)
            mesg += QString(" 1/4x");
        else if (ts_normal == 0.125)
            mesg += QString(" 1/8x");
        else if (ts_normal == 0.0625)
            mesg += QString(" 1/16x");
        else
            mesg += QString(" %1x").arg(ts_normal);
    }

    return mesg;
}

void PlayerContext::SetPlayer(MythPlayer *newplayer)
{
    QMutexLocker locker(&deletePlayerLock);
    if (player)
    {
        StopPlaying();
        delete player;
    }
    player = newplayer;
}

void PlayerContext::SetRecorder(RemoteEncoder *rec)
{
    if (recorder)
    {
        delete recorder;
        recorder = NULL;
    }

    if (rec)
    {
        recorder = rec;
        last_cardid = recorder->GetRecorderNumber();
    }
}

void PlayerContext::SetTVChain(LiveTVChain *chain)
{
    if (tvchain)
    {
        tvchain->DestroyChain();
        delete tvchain;
        tvchain = NULL;
    }

    tvchain = chain;

    if (tvchain)
    {
        QString seed = QString("");

        if (IsPIP())
            seed = "PIP";

        seed += gCoreContext->GetHostName();

        tvchain->InitializeNewChain(gCoreContext->GetHostName());
    }
}

void PlayerContext::SetRingBuffer(RingBuffer *buf)
{
    if (buffer)
    {
        delete buffer;
        buffer = NULL;
    }

    buffer = buf;
}

/**
 * \brief assign programinfo to the context
 */
void PlayerContext::SetPlayingInfo(const ProgramInfo *info)
{
    bool ignoreDB = gCoreContext->IsDatabaseIgnored();

    QMutexLocker locker(&playingInfoLock);

    if (playingInfo)
    {
        if (!ignoreDB)
            playingInfo->MarkAsInUse(false, recUsage);
        delete playingInfo;
        playingInfo = NULL;
    }

    if (info)
    {
        playingInfo = new ProgramInfo(*info);
        if (!ignoreDB)
            playingInfo->MarkAsInUse(true, recUsage);
        playingLen = playingInfo->GetSecondsInRecording();
    }
}

void PlayerContext::SetPlayGroup(const QString &group)
{
    fftime       = PlayGroup::GetSetting(group, "skipahead", 30);
    rewtime      = PlayGroup::GetSetting(group, "skipback", 5);
    jumptime     = PlayGroup::GetSetting(group, "jump", 10);
    ts_normal    = PlayGroup::GetSetting(group, "timestretch", 100) * 0.01f;
    ts_alt       = (ts_normal == 1.0f) ? 1.5f : 1.0f;
}

void PlayerContext::SetPseudoLiveTV(
    const ProgramInfo *pi, PseudoState new_state)
{
    ProgramInfo *old_rec = pseudoLiveTVRec;
    ProgramInfo *new_rec = NULL;

    if (pi)
    {
        new_rec = new ProgramInfo(*pi);
        QString msg = QString("Wants to record: %1 %2 %3 %4")
            .arg(new_rec->GetTitle()).arg(new_rec->GetChanNum())
            .arg(new_rec->GetRecordingStartTime(MythDate::ISODate))
            .arg(new_rec->GetRecordingEndTime(MythDate::ISODate));
        LOG(VB_PLAYBACK, LOG_INFO, LOC + msg);
    }

    pseudoLiveTVRec   = new_rec;
    pseudoLiveTVState = new_state;

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
