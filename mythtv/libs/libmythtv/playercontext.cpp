#include <cmath>

#include <QPainter>

#include "mythconfig.h"

#include "playercontext.h"
#include "NuppelVideoPlayer.h"
#include "mythdvdplayer.h"
#include "remoteencoder.h"
#include "livetvchain.h"
#include "RingBuffer.h"
#include "playgroup.h"
#include "util-osx-cocoa.h"
#include "videoouttypes.h"
#include "storagegroup.h"
#include "mythcorecontext.h"
#include "videometadatautil.h"

#define LOC QString("playCtx: ")
#define LOC_ERR QString("playCtx, Error: ")

const uint PlayerContext::kSMExitTimeout     = 2000;
const uint PlayerContext::kMaxChannelHistory = 30;

void PlayerThread::run(void)
{
    if (!m_nvp)
        return;

    VERBOSE(VB_PLAYBACK, QString("PiP thread starting"));
    m_nvp->StartPlaying();
    exec();
    VERBOSE(VB_PLAYBACK, QString("PiP thread finishing"));
}

PlayerContext::PlayerContext(const QString &inUseID) :
    recUsage(inUseID), nvp(NULL), nvpUnsafe(false), recorder(NULL),
    tvchain(NULL), buffer(NULL), playingInfo(NULL),
    playingLen(0), nohardwaredecoders(false), last_cardid(-1), last_framerate(30.0f),
    // Fast forward state
    ff_rew_state(0), ff_rew_index(0), ff_rew_speed(0),
    // Other state
    paused(false), playingState(kState_None),
    errored(false),
    // pseudo states
    pseudoLiveTVRec(NULL), pseudoLiveTVState(kPseudoNormalLiveTV),
    // DB values
    fftime(0), rewtime(0),
    jumptime(0), ts_normal(1.0f), ts_alt(1.5f),
    // locks
    playingInfoLock(QMutex::Recursive), deleteNVPLock(QMutex::Recursive),
    stateLock(QMutex::Recursive),
    // pip
    pipState(kPIPOff), pipRect(0,0,0,0), parentWidget(NULL), pipLocation(0),
    useNullVideo(false), playerNeedsThread(false), playerThread(NULL),
    // embedding
    embedWinID(0), embedBounds(0,0,0,0)
{
    lastSignalMsgTime.start();
    lastSignalMsgTime.addMSecs(-2 * kSMExitTimeout);
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

    SetNVP(NULL);
    SetRecorder(NULL);
    SetRingBuffer(NULL);
    SetTVChain(NULL);
    SetPlayingInfo(NULL);
    DeletePlayerThread();
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
        QDateTime curtime = QDateTime::currentDateTime();
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
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        const VideoOutput *vid = nvp->getVideoOutput();
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
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        const VideoOutput *vid = nvp->getVideoOutput();
        if (vid)
            supported = vid->IsPBPSupported();
    }
    return supported;
}

bool PlayerContext::IsOSDFullScreen(void) const
{
    // Note: This is to allow future OSD implementations to cover
    // two or more PBP screens.
    return false;
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
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        rect = QRect(pipRect);

        float saspect = (float)rect.width() / (float)rect.height();
        float vaspect = nvp->GetVideoAspect();

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
    playerNeedsThread = true;

    if (!useNullVideo && parentWidget)
    {
        const QRect rect = QRect(pipRect);
        ok = CreateNVP(tv, parentWidget, desiredState,
                     parentWidget->winId(), &rect);
    }

    if (useNullVideo || !ok)
    {
        SetNVP(NULL);
        useNullVideo = true;
        ok = CreateNVP(tv, NULL, desiredState,
                        0, NULL);
    }

    playerNeedsThread = false;
    return ok;
}


/**
 * \brief stop NVP but pause the ringbuffer. used in PIP/PBP swap or
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
        QMutexLocker locker(&deleteNVPLock);
        StopPlaying();
    }

    SetNVP(NULL);

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

    LockDeleteNVP(__FILE__, __LINE__);
    if (nvp && nvp->getVideoOutput())
        nvp->getVideoOutput()->ResizeDisplayWindow(tmpRect, false);
    UnlockDeleteNVP(__FILE__, __LINE__);

    pipRect = QRect(rect);
}

bool PlayerContext::StartEmbedding(WId wid, const QRect &embedRect)
{
    embedWinID = 0;

    LockDeleteNVP(__FILE__, __LINE__);
    if (nvp)
    {
        embedWinID = wid;
        embedBounds = embedRect;
        nvp->EmbedInWidget(
            embedRect.topLeft().x(), embedRect.topLeft().y(),
            embedRect.width(),       embedRect.height(),
            embedWinID);
    }
    UnlockDeleteNVP(__FILE__, __LINE__);

    return embedWinID;
}

bool PlayerContext::IsEmbedding(void) const
{
    bool ret = false;
    LockDeleteNVP(__FILE__, __LINE__);
    if (nvp)
        ret = nvp->IsEmbedding();
    UnlockDeleteNVP(__FILE__, __LINE__);
    return ret;
}

void PlayerContext::StopEmbedding(void)
{
    embedWinID = 0;

    LockDeleteNVP(__FILE__, __LINE__);
    if (nvp)
        nvp->StopEmbedding();
    UnlockDeleteNVP(__FILE__, __LINE__);
}

bool PlayerContext::HasNVP(void) const
{
    QMutexLocker locker(&deleteNVPLock);
    return nvp;
}

bool PlayerContext::IsNVPErrored(void) const
{
    QMutexLocker locker(&deleteNVPLock);
    return nvp && nvp->IsErrored();
}

bool PlayerContext::IsNVPRecoverable(void) const
{
    QMutexLocker locker(&deleteNVPLock);
    return nvp && nvp->IsErrorRecoverable();
}

bool PlayerContext::IsNVPDecoderErrored(void) const
{
    QMutexLocker locker(&deleteNVPLock);
    return nvp && nvp->IsDecoderErrored();
}

bool PlayerContext::IsNVPPlaying(void) const
{
    QMutexLocker locker(&deleteNVPLock);
    return nvp && nvp->IsPlaying();
}

bool PlayerContext::HandleNVPSpeedChangeFFRew(void)
{
    QMutexLocker locker(&deleteNVPLock);
    if ((ff_rew_state || ff_rew_speed) && nvp && nvp->AtNormalSpeed())
    {
        ff_rew_speed = 0;
        ff_rew_state = 0;
        ff_rew_index = TV::kInitFFRWSpeed;
        return true;
    }
    return false;
}

bool PlayerContext::HandleNVPSpeedChangeEOF(void)
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp && (nvp->GetNextPlaySpeed() != ts_normal) && nvp->AtNormalSpeed())
    {
        // Speed got changed in NVP since we are close to the end of file
        ts_normal = 1.0f;
        return true;
    }
    return false;
}

bool PlayerContext::CalcNVPSliderPosition(osdInfo &info,
                                          bool paddedFields) const
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        nvp->calcSliderPos(info);
        return true;
    }
    return false;
}

bool PlayerContext::IsRecorderErrored(void) const
{
    return recorder && recorder->GetErrorStatus();
}

bool PlayerContext::CreateNVP(TV *tv, QWidget *widget,
                              TVState desiredState,
                              WId embedwinid, const QRect *embedbounds,
                              bool muted)
{
    int exact_seeking = gCoreContext->GetNumSetting("ExactSeeking", 0);

    if (HasNVP())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Attempting to setup a player, but it already exists.");
        return false;
    }

    NuppelVideoPlayer *_nvp = NULL;
    if (kState_WatchingDVD == desiredState)
        _nvp = new MythDVDPlayer(muted);
    else
        _nvp = new NuppelVideoPlayer(muted);

    if (nohardwaredecoders)
        _nvp->DisableHardwareDecoders();

    QString passthru_device = gCoreContext->GetNumSetting("AdvancedAudioSettings", false) &&
                              gCoreContext->GetNumSetting("PassThruDeviceOverride", false) ?
                                  gCoreContext->GetSetting("PassThruOutputDevice") : QString::null;

    _nvp->SetPlayerInfo(tv, widget, exact_seeking, this);
    AudioPlayer *audio = _nvp->GetAudio();
    audio->SetAudioInfo(gCoreContext->GetSetting("AudioOutputDevice"),
                        passthru_device,
                        gCoreContext->GetNumSetting("AudioSampleRate", 44100));
    audio->SetStretchFactor(ts_normal);
    _nvp->SetLength(playingLen);

    if (useNullVideo)
        _nvp->SetNullVideo();

    _nvp->SetVideoFilters((useNullVideo) ? "onefield" : "");

    if (!IsAudioNeeded())
        audio->SetNoAudio();
    else
    {
        QString subfn = buffer->GetSubtitleFilename();
        if (!subfn.isEmpty() && _nvp->GetSubReader())
            _nvp->GetSubReader()->LoadExternalSubtitles(subfn);
    }

    if ((embedwinid > 0) && embedbounds)
    {
        _nvp->EmbedInWidget(
            embedbounds->x(), embedbounds->y(),
            embedbounds->width(), embedbounds->height(), embedwinid);
    }

    bool isWatchingRecording = (desiredState == kState_WatchingRecording);
    _nvp->SetWatchingRecording(isWatchingRecording);

    SetNVP(_nvp);

    if (pipState == kPIPOff || pipState == kPBPLeft)
    {
        if (audio->HasAudioOut())
        {
            QString errMsg = audio->ReinitAudio();
            if (!errMsg.isEmpty())
                VERBOSE(VB_IMPORTANT, LOC_ERR + errMsg);
        }
    }
    else if (pipState == kPBPRight)
        nvp->SetMuted(true);

    return StartPlaying(-1);
}

/** \fn PlayerContext::StartPlaying(int)
 *  \brief Starts player, must be called after StartRecorder().
 *  \param maxWait How long to wait for NuppelVideoPlayer to start playing.
 *  \return true when successful, false otherwise.
 */
bool PlayerContext::StartPlaying(int maxWait)
{
    if (!nvp)
        return false;

    DeletePlayerThread();
    if (playerNeedsThread)
    {
        playerThread = new PlayerThread(nvp);
        if (playerThread)
            playerThread->start();
    }
    else
    {
        nvp->StartPlaying();
    }

    maxWait = (maxWait <= 0) ? 20000 : maxWait;
#ifdef USING_VALGRIND
    maxWait = (1<<30);
#endif // USING_VALGRIND
    MythTimer t;
    t.start();

    while (!nvp->IsPlaying(50, true) && (t.elapsed() < maxWait))
        ReloadTVChain();

    if (nvp->IsPlaying())
    {
        VERBOSE(VB_PLAYBACK, LOC + "StartPlaying(): took "<<t.elapsed()
                <<" ms to start player.");
        return true;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "StartPlaying() "
                "Failed to start player");
        StopPlaying();
        return false;
    }
}

void PlayerContext::StopPlaying(void)
{
    DeletePlayerThread();
    if (nvp)
        nvp->StopPlaying();
}

void PlayerContext::DeletePlayerThread(void)
{
    if (playerThread)
    {
        playerThread->exit();
        playerThread->wait();
        delete playerThread;
        playerThread = NULL;
    }
}

/** \fn PlayerContext::StartOSD(TV *tv)
 *  \brief Initializes the on screen display.
 *
 *   If the NuppelVideoPlayer already exists we grab it's OSD via
 *   NuppelVideoPlayer::GetOSD().
 */
bool PlayerContext::StartOSD(TV *tv)
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        last_framerate = nvp->GetFrameRate();

        OSD *osd = nvp->GetOSD();
        if (osd)
            return true;
    }
    return false;
}

void PlayerContext::UpdateTVChain(void)
{
    QMutexLocker locker(&deleteNVPLock);
    if (tvchain && nvp)
    {
        tvchain->ReloadAll();
        nvp->CheckTVChain();
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
    if (prevChan.size() == 0 ||
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
    //VERBOSE(VB_IMPORTANT, QString("LockPlayingInfo(%1,%2)")
    //        .arg(file).arg(line));
    playingInfoLock.lock();
}

void PlayerContext::UnlockPlayingInfo(const char *file, int line) const
{
    //VERBOSE(VB_IMPORTANT, QString("UnlockPlayingInfo(%1,%2)")
    //        .arg(file).arg(line));
    playingInfoLock.unlock();
}

/**
 * \brief prevent NVP from been deleted
 *        used to ensure nvp can only be deleted after
 *        osd in TV() is unlocked.
 */
void PlayerContext::LockDeleteNVP(const char *file, int line) const
{
    //VERBOSE(VB_IMPORTANT, QString("LockDeleteNVP(%1,%2)")
    //        .arg(file).arg(line));
    deleteNVPLock.lock();
}

/**
 * \brief allow NVP to be deleted.
 */
void PlayerContext::UnlockDeleteNVP(const char *file, int line) const
{
    //VERBOSE(VB_IMPORTANT, QString("UnlockDeleteNVP(%1,%2)")
    //        .arg(file).arg(line));
    deleteNVPLock.unlock();
}

void PlayerContext::LockState(void) const
{
    stateLock.lock();
}

void PlayerContext::UnlockState(void) const
{
    stateLock.unlock();
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
        infoMap["iconpath"] = ChannelUtil::GetIcon(playingInfo->GetChanID());
        if (playingInfo->IsVideoFile() &&
            playingInfo->GetPathname() != playingInfo->GetBasename())
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
            if (!filters.isEmpty() && (filters.right(1) != ","))
                filters += ",";

            filters += chanFilters.mid(1);
        }
    }

    VERBOSE(VB_CHANNEL, LOC +
            QString("Output filters for this channel are: '%1'")
                    .arg(filters));

    filters.detach();
    return filters;
}

QString PlayerContext::GetPlayMessage(void) const
{
    QString mesg = QObject::tr("Play");
    if (ts_normal != 1.0)
        mesg += QString(" %1X").arg(ts_normal);

    if (0)
    {
        QMutexLocker locker(&deleteNVPLock);
        FrameScanType scan = nvp->GetScanType();
        if (is_progressive(scan) || is_interlaced(scan))
            mesg += " (" + toString(scan, true) + ")";
    }

    return mesg;
}

void PlayerContext::SetNVP(NuppelVideoPlayer *new_nvp)
{
    QMutexLocker locker(&deleteNVPLock);
    if (nvp)
    {
        StopPlaying();
        delete nvp;
    }
    nvp = new_nvp;
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
            .arg(new_rec->GetRecordingStartTime(ISODate))
            .arg(new_rec->GetRecordingEndTime(ISODate));
        VERBOSE(VB_PLAYBACK, LOC + msg);
    }

    pseudoLiveTVRec   = new_rec;
    pseudoLiveTVState = new_state;

    if (old_rec)
    {
        QString msg = QString("Done recording: %1 %2 %3 %4")
            .arg(old_rec->GetTitle()).arg(old_rec->GetChanNum())
            .arg(old_rec->GetRecordingStartTime(ISODate))
            .arg(old_rec->GetRecordingEndTime(ISODate));
        VERBOSE(VB_PLAYBACK, LOC + msg);
        delete old_rec;
    }
}
