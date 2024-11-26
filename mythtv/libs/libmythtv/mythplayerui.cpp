#include <algorithm>

// MythTV
#include "libmyth/audio/audiooutput.h"
#include "libmythui/mythmainwindow.h"

#include "decoders/avformatdecoder.h"
#include "io/mythinteractivebuffer.h"
#include "livetvchain.h"
#include "mheg/interactivescreen.h"
#include "mheg/interactivetv.h"
#include "mythplayerui.h"
#include "mythsystemevent.h"
#include "osd.h"
#include "tv_play.h"

#define LOC QString("PlayerUI: ")

MythPlayerUI::MythPlayerUI(MythMainWindow* MainWindow, TV* Tv,
                           PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerEditorUI(MainWindow, Tv, Context, Flags),
    MythVideoScanTracker(this),
    m_display(MainWindow->GetDisplay())
{
    // Finish setting up the overlays
    m_osd.SetPlayer(this);
    m_captionsOverlay.SetPlayer(this);

    // User feedback during slow seeks
    connect(this, &MythPlayerUI::SeekingSlow, [&](int Count)
    {
        UpdateOSDMessage(tr("Searching") + QString().fill('.', Count % 3), kOSDTimeout_Short);
        DisplayPauseFrame();
    });

    // Seeking has finished; remove slow seek user feedback window
    connect(this, &MythPlayerUI::SeekingComplete, [&]()
    {
        m_osdLock.lock();
        m_osd.HideWindow(OSD_WIN_MESSAGE);
        m_osdLock.unlock();
    });

    // Seeking has finished; update position on OSD
    connect(this, &MythPlayerUI::SeekingDone, [&]()
    {
        UpdateOSDPosition();
    });

    // Setup OSD debug
    m_osdDebugTimer.setInterval(1s);
    connect(&m_osdDebugTimer, &QTimer::timeout, this, &MythPlayerUI::UpdateOSDDebug);
    connect(m_tv, &TV::ChangeOSDDebug, this, &MythPlayerUI::ChangeOSDDebug);

    // Other connections
    connect(m_tv, &TV::UpdateBookmark, this, &MythPlayerUI::SetBookmark);
    connect(m_tv, &TV::UpdateLastPlayPosition, this, &MythPlayerUI::SetLastPlayPosition);
    connect(m_tv, &TV::InitialisePlayerState, this, &MythPlayerUI::InitialiseState);

    // Setup refresh interval.
    m_refreshInterval = m_display->GetRefreshInterval(16667us);
    m_avSync.SetRefreshInterval(m_refreshInterval);
}

void MythPlayerUI::InitialiseState()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Initialising player state");
    MythPlayerEditorUI::InitialiseState();
}

bool MythPlayerUI::StartPlaying()
{
    if (OpenFile() < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to open video file.");
        return false;
    }

    m_framesPlayed = 0;
    m_rewindTime = m_ffTime = 0;
    m_nextPlaySpeed = m_audio.GetStretchFactor();
    m_jumpChapter = 0;
    m_commBreakMap.SkipCommercials(0);
    m_bufferingCounter = 0;

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to initialize video.");
        m_audio.DeleteOutput();
        return false;
    }

    EventStart();
    DecoderStart(true);
    InitialSeek();
    VideoStart();

    m_playerThread->setPriority(QThread::TimeCriticalPriority);
#ifdef Q_OS_ANDROID
    setpriority(PRIO_PROCESS, m_playerThreadId, -20);
#endif
    ProcessCallbacks();
    UnpauseDecoder();
    return !IsErrored();
}

void MythPlayerUI::InitialSeek()
{
    // TODO handle initial commskip and/or cutlist skip as well
    if (m_bookmarkSeek > 30)
        DoJumpToFrame(m_bookmarkSeek, kInaccuracyNone);
}

void MythPlayerUI::EventLoop()
{
    // Handle decoder callbacks
    ProcessCallbacks();

    // Live TV program change
    if (m_fileChanged)
        FileChanged();

    // Check if visualiser is wanted on first start and after video change
    if (m_checkAutoVisualise)
        AutoVisualise(!m_videoDim.isEmpty());

    // recreate the osd if a reinit was triggered by another thread
    if (m_reinitOsd)
        ReinitOSD();

    // enable/disable forced subtitles if signalled by the decoder
    if (m_enableForcedSubtitles)
        DoEnableForcedSubtitles();
    if (m_disableForcedSubtitles)
        DoDisableForcedSubtitles();

    // reset the scan (and hence deinterlacers) if triggered by the decoder
    CheckScanUpdate(m_videoOutput, m_frameInterval);

    // refresh the position map for an in-progress recording while editing
    if (m_hasFullPositionMap && IsWatchingInprogress() && m_deleteMap.IsEditing())
    {
        if (m_editUpdateTimer.hasExpired(2000))
        {
            // N.B. the positionmap update and osd refresh are asynchronous
            m_forcePositionMapSync = true;
            m_osdLock.lock();
            m_deleteMap.UpdateOSD(m_framesPlayed, m_videoFrameRate, &m_osd);
            m_osdLock.unlock();
            m_editUpdateTimer.start();
        }
    }

    // Refresh the programinfo in use status
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->UpdateInUseMark();
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    // Disable timestretch if we are too close to the end of the buffer
    if (m_ffrewSkip == 1 && (m_playSpeed > 1.0F) && IsNearEnd())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Near end, Slowing down playback.");
        Play(1.0F, true, true);
    }

    if (m_isDummy && m_playerCtx->m_tvchain && m_playerCtx->m_tvchain->HasNext())
    {
        // Switch from the dummy recorder to the tuned program in livetv
        m_playerCtx->m_tvchain->JumpToNext(true, 0s);
        JumpToProgram();
    }
    else if ((!m_allPaused || GetEof() != kEofStateNone) &&
             m_playerCtx->m_tvchain &&
             (m_decoder && !m_decoder->GetWaitForChange()))
    {
        // Switch to the next program in livetv
        if (m_playerCtx->m_tvchain->NeedsToSwitch())
            SwitchToProgram();
    }

    // Jump to the next program in livetv
    if (m_playerCtx->m_tvchain && m_playerCtx->m_tvchain->NeedsToJump())
    {
        JumpToProgram();
    }

    // Change interactive stream if requested
    if (!m_newStream.isEmpty())
    {
        JumpToStream(m_newStream);
        m_newStream = QString();
    }

    // Disable fastforward if we are too close to the end of the buffer
    if (m_ffrewSkip > 1 && (CalcMaxFFTime(100, false) < 100))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Near end, stopping fastforward.");
        Play(1.0F, true, true);
    }

    // Disable rewind if we are too close to the beginning of the buffer
    if (m_ffrewSkip < 0 && CalcRWTime(-m_ffrewSkip) >= 0 &&
        (m_framesPlayed <= m_keyframeDist))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Near start, stopping rewind.");
        float stretch = (m_ffrewSkip > 0) ? 1.0F : m_audio.GetStretchFactor();
        Play(stretch, true, true);
    }

    // Check for error
    if (IsErrored() || m_playerCtx->IsRecorderErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown recorder error, exiting decoder");
        if (!IsErrored())
            SetErrored(tr("Irrecoverable recorder error"));
        m_killDecoder = true;
        return;
    }

    // Handle speed change
    if (!qFuzzyCompare(m_playSpeed + 1.0F, m_nextPlaySpeed + 1.0F) &&
        (!m_playerCtx->m_tvchain || !m_playerCtx->m_tvchain->NeedsToJump()))
    {
        ChangeSpeed();
        return;
    }

    // Check if we got a communication error, and if so pause playback
    if (m_playerCtx->m_buffer->GetCommsError())
    {
        Pause();
        m_playerCtx->m_buffer->ResetCommsError();
    }

    // Handle end of file
    EofState eof = GetEof();
    if (HasReachedEof())
    {
#ifdef USING_MHEG
        if (m_interactiveTV && m_interactiveTV->StreamStarted(false))
        {
            Pause();
            return;
        }
#endif
        if (m_playerCtx->m_tvchain && m_playerCtx->m_tvchain->HasNext())
        {
            LOG(VB_GENERAL, LOG_NOTICE, LOC + "LiveTV forcing JumpTo 1");
            m_playerCtx->m_tvchain->JumpToNext(true, 0s);
            return;
        }

        bool drained = false;
        if (FlagIsSet(kVideoIsNull) || FlagIsSet(kMusicChoice))
        {
            // We have audio only or mostly audio content.  Exit when
            // the audio is drained.
            drained =
                !m_audio.GetAudioOutput() ||
                m_audio.IsPaused() ||
                m_audio.GetAudioOutput()->GetAudioBufferedTime() < 100ms;
        }
        else
        {
            // We have normal or video only content.  Exit when the
            // video is drained.
            drained = m_videoOutput && !m_videoOutput->ValidVideoFrames();
        }
        if (eof != kEofStateDelayed || drained)
        {
            if (eof == kEofStateDelayed)
            {
                LOG(VB_PLAYBACK, LOG_INFO,
                    QString("waiting for no video frames %1")
                    .arg(m_videoOutput->ValidVideoFrames()));
            }
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("HasReachedEof() at framesPlayed=%1 totalFrames=%2")
                .arg(m_framesPlayed).arg(GetCurrentFrameCount()));
            Pause();
            SetPlaying(false);
            return;
        }
    }

    // Handle rewind
    if (m_rewindTime > 0 && (m_ffrewSkip == 1 || m_ffrewSkip == 0))
    {
        m_rewindTime = CalcRWTime(m_rewindTime);
        if (m_rewindTime > 0)
            DoRewind(static_cast<uint64_t>(m_rewindTime), kInaccuracyDefault);
    }

    // Handle fast forward
    if (m_ffTime > 0 && (m_ffrewSkip == 1 || m_ffrewSkip == 0))
    {
        m_ffTime = CalcMaxFFTime(m_ffTime);
        if (m_ffTime > 0)
        {
            DoFastForward(static_cast<uint64_t>(m_ffTime), kInaccuracyDefault);
            if (GetEof() != kEofStateNone)
               return;
        }
    }

    // Handle chapter jump
    if (m_jumpChapter != 0)
        DoJumpChapter(m_jumpChapter);

    // Handle commercial skipping
    if (m_commBreakMap.GetSkipCommercials() != 0 && (m_ffrewSkip == 1))
    {
        if (!m_commBreakMap.HasMap())
        {
            //: The commercials/adverts have not been flagged
            SetOSDStatus(tr("Not Flagged"), kOSDTimeout_Med);
            QString message = "COMMFLAG_REQUEST ";
            m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
            message += QString("%1").arg(m_playerCtx->m_playingInfo->GetChanID()) +
                " " + m_playerCtx->m_playingInfo->MakeUniqueKey();
            m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
            gCoreContext->SendMessage(message);
        }
        else
        {
            QString msg;
            uint64_t jumpto = 0;
            uint64_t frameCount = GetCurrentFrameCount();
            // XXX CommBreakMap should use duration map not m_videoFrameRate
            bool jump = m_commBreakMap.DoSkipCommercials(jumpto, m_framesPlayed,
                                                         m_videoFrameRate,
                                                         frameCount, msg);
            if (!msg.isEmpty())
                SetOSDStatus(msg, kOSDTimeout_Med);
            if (jump)
                DoJumpToFrame(jumpto, kInaccuracyNone);
        }
        m_commBreakMap.SkipCommercials(0);
        return;
    }

    // Handle automatic commercial skipping
    uint64_t jumpto = 0;
    if (m_deleteMap.IsEmpty() && (m_ffrewSkip == 1) &&
       (kCommSkipOff != m_commBreakMap.GetAutoCommercialSkip()) &&
        m_commBreakMap.HasMap())
    {
        QString msg;
        uint64_t frameCount = GetCurrentFrameCount();
        // XXX CommBreakMap should use duration map not m_videoFrameRate
        bool jump = m_commBreakMap.AutoCommercialSkip(jumpto, m_framesPlayed,
                                                      m_videoFrameRate,
                                                      frameCount, msg);
        if (!msg.isEmpty())
            SetOSDStatus(msg, kOSDTimeout_Med);
        if (jump)
            DoJumpToFrame(jumpto, kInaccuracyNone);
    }

    // Handle cutlist skipping
    if (!m_allPaused && (m_ffrewSkip == 1) &&
        m_deleteMap.TrackerWantsToJump(m_framesPlayed, jumpto))
    {
        if (jumpto == m_totalFrames)
        {
            if (m_endExitPrompt != 1 || m_playerCtx->GetState() != kState_WatchingPreRecorded)
                SetEof(kEofStateDelayed);
        }
        else
        {
            DoJumpToFrame(jumpto, kInaccuracyNone);
        }
    }
}

void MythPlayerUI::PreProcessNormalFrame()
{
#ifdef USING_MHEG
    // handle Interactive TV
    if (GetInteractiveTV())
    {
        m_osdLock.lock();
        m_itvLock.lock();
        auto *window = qobject_cast<InteractiveScreen *>(m_captionsOverlay.GetWindow(OSD_WIN_INTERACT));
        if ((m_interactiveTV->ImageHasChanged() || !m_itvVisible) && window)
        {
            m_interactiveTV->UpdateOSD(window, m_painter);
            m_itvVisible = true;
        }
        m_itvLock.unlock();
        m_osdLock.unlock();
    }
#endif // USING_MHEG
}

void MythPlayerUI::ChangeSpeed()
{
    MythPlayer::ChangeSpeed();
    // ensure we re-check double rate support following a speed change
    UnlockScan();
}

void MythPlayerUI::ReinitVideo(bool ForceUpdate)
{
    MythPlayer::ReinitVideo(ForceUpdate);

    // Signal to the main thread to reinit OSD
    m_reinitOsd = true;

    // Signal to main thread to reinit subtitles
    if (m_captionsState.m_textDisplayMode != kDisplayNone)
        emit EnableSubtitles(true);

    // Signal to the main thread to check auto visualise
    m_checkAutoVisualise = true;
}

void MythPlayerUI::VideoStart()
{
    QRect visible;
    QRect total;
    float aspect = NAN;
    float scaling = NAN;

    m_osdLock.lock();
    m_videoOutput->GetOSDBounds(total, visible, aspect, scaling, 1.0F);
    m_osd.Init(visible, aspect);
    m_captionsOverlay.Init(visible, aspect);
    m_captionsOverlay.EnableSubtitles(kDisplayNone);

#ifdef USING_MHEG
    if (GetInteractiveTV())
    {
        QMutexLocker locker(&m_itvLock);
        m_interactiveTV->Reinit(total, visible, aspect);
    }
#endif // USING_MHEG

    // If there is a forced text subtitle track (which is possible
    // in e.g. a .mkv container), and forced subtitles are
    // allowed, then start playback with that subtitle track
    // selected.  Otherwise, use the frontend settings to decide
    // which captions/subtitles (if any) to enable at startup.
    // TODO: modify the fix to #10735 to use this approach
    // instead.
    bool hasForcedTextTrack = false;
    uint forcedTrackNumber = 0;
    if (GetAllowForcedSubtitles())
    {
        uint numTextTracks = m_decoder->GetTrackCount(kTrackTypeRawText);
        for (uint i = 0; !hasForcedTextTrack && i < numTextTracks; ++i)
        {
            if (m_decoder->GetTrackInfo(kTrackTypeRawText, i).m_forced)
            {
                hasForcedTextTrack = true;
                forcedTrackNumber = i;
            }
        }
    }
    if (hasForcedTextTrack)
        SetTrack(kTrackTypeRawText, forcedTrackNumber);
    else
        SetCaptionsEnabled(m_captionsEnabledbyDefault, false);

    m_osdLock.unlock();

    SetPlaying(true);
    ClearAfterSeek(false);
    m_avSync.InitAVSync();
    InitFrameInterval();

    InitialiseScan(m_videoOutput);
    EnableFrameRateMonitor();
    AutoVisualise(!m_videoDim.isEmpty());
}

void MythPlayerUI::EventStart()
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    {
        if (m_playerCtx->m_playingInfo)
        {
            // When initial playback gets underway, we override the ProgramInfo
            // flags such that future calls to GetBookmark() will consider only
            // an actual bookmark and not progstart or lastplaypos information.
            m_playerCtx->m_playingInfo->SetIgnoreBookmark(false);
            m_playerCtx->m_playingInfo->SetIgnoreProgStart(true);
            m_playerCtx->m_playingInfo->SetIgnoreLastPlayPos(true);
        }
    }
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    m_commBreakMap.LoadMap(m_playerCtx, m_framesPlayed);
}

bool MythPlayerUI::VideoLoop()
{
    ProcessCallbacks();

    if (m_videoPaused || m_isDummy)
        DisplayPauseFrame();
    else if (DisplayNormalFrame())
    {
        if (FlagIsSet(kVideoIsNull) && m_decoder)
            m_decoder->UpdateFramesPlayed();
        else if (m_decoder && m_decoder->GetEof() != kEofStateNone)
            ++m_framesPlayed;
        else
            m_framesPlayed = static_cast<uint64_t>(
                m_videoOutput->GetFramesPlayed());
    }
    
    return !IsErrored();
}

void MythPlayerUI::InitFrameInterval()
{
    SetFrameInterval(GetScanType(), 1.0 / (m_videoFrameRate * static_cast<double>(m_playSpeed)));
    MythPlayer::InitFrameInterval();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Display Refresh Rate: %1 Video Frame Rate: %2")
        .arg(1000000.0 / m_refreshInterval.count(), 0, 'f', 3)
        .arg(1000000.0 / m_frameInterval.count(), 0, 'f', 3));
}

void MythPlayerUI::RenderVideoFrame(MythVideoFrame *Frame, FrameScanType Scan, bool Prepare, std::chrono::microseconds Wait)
{
    if (!m_videoOutput)
        return;

    if (Prepare)
        m_videoOutput->PrepareFrame(Frame, Scan);
    PrepareVisualiser();
    m_videoOutput->RenderFrame(Frame, Scan);
    if (m_renderOneFrame)
        LOG(VB_PLAYBACK, LOG_DEBUG, QString("Clearing render one"));
    m_renderOneFrame = false;
    RenderVisualiser();
    m_captionsOverlay.Draw(m_videoOutput->GetDisplayVisibleRect());
    m_osd.Draw();
    m_videoOutput->RenderEnd();

    if (Wait > 0us)
        m_avSync.WaitForFrame(Wait);

    m_videoOutput->EndFrame();
}

void MythPlayerUI::FileChanged()
{
    m_fileChanged = false;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "FileChanged");

    Pause();
    ChangeSpeed();
    if (dynamic_cast<AvFormatDecoder *>(m_decoder))
        m_playerCtx->m_buffer->Reset(false, true);
    else
        m_playerCtx->m_buffer->Reset(false, true, true);
    SetEof(kEofStateNone);
    Play();

    m_playerCtx->SetPlayerChangingBuffers(false);

    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    m_playerCtx->m_tvchain->SetProgram(*m_playerCtx->m_playingInfo);
    if (m_decoder)
        m_decoder->SetProgramInfo(*m_playerCtx->m_playingInfo);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    CheckTVChain();
    m_forcePositionMapSync = true;
}

void MythPlayerUI::RefreshPauseFrame()
{
    if (m_needNewPauseFrame)
    {
        if (m_videoOutput->ValidVideoFrames())
        {
            m_videoOutput->UpdatePauseFrame(m_avSync.DisplayTimecode(), GetScanType());
            m_needNewPauseFrame = false;

            if (m_deleteMap.IsEditing())
            {
                m_osdLock.lock();
                DeleteMap::UpdateOSD(m_latestVideoTimecode, &m_osd);
                m_osdLock.unlock();
            }
        }
        else
        {
            m_decodeOneFrame = true;
        }
    }
}

void MythPlayerUI::DoDisplayVideoFrame(MythVideoFrame* Frame, std::chrono::microseconds Due)
{
    if (Due < 0us)
    {
        m_videoOutput->SetFramesPlayed(static_cast<long long>(++m_framesPlayed));
        if (m_renderOneFrame)
            LOG(VB_PLAYBACK, LOG_DEBUG, QString("Clearing render one"));
        m_renderOneFrame = false;
    }
    else if (!FlagIsSet(kVideoIsNull) && Frame)
    {

        // Check scan type
        bool showsecondfield = false;
        FrameScanType ps = GetScanForDisplay(Frame, showsecondfield);

        // if we get here, we're actually going to do video output
        RenderVideoFrame(Frame, ps, true, Due);

        // Only double rate CPU deinterlacers require an extra call to PrepareFrame
        bool secondprepare = Frame->GetDoubleRateOption(DEINT_CPU) && !Frame->GetDoubleRateOption(DEINT_SHADER);
        // and the first deinterlacing pass will have marked the frame as already deinterlaced
        // which will break GetScanForDisplay below and subsequent deinterlacing
        bool olddeinterlaced = Frame->m_alreadyDeinterlaced;
        if (secondprepare)
            Frame->m_alreadyDeinterlaced = false;
        // Update scan settings now that deinterlacer has been set and we know
        // whether we need a second field
        ps = GetScanForDisplay(Frame, showsecondfield);

        // Reset olddeinterlaced if necessary (pause frame etc)
        if (!showsecondfield && secondprepare)
        {
            Frame->m_alreadyDeinterlaced = olddeinterlaced;
        }
        else if (showsecondfield)
        {
            // Second field
            if (kScan_Interlaced == ps)
                ps = kScan_Intr2ndField;
            RenderVideoFrame(Frame, ps, secondprepare, Due + m_frameInterval / 2);
        }
    }
    else
    {
        m_avSync.WaitForFrame(Due);
    }
}

void MythPlayerUI::DisplayPauseFrame()
{
    if (!m_videoOutput)
        return;

    if (m_videoOutput->IsErrored())
    {
        SetErrored(tr("Serious error detected in Video Output"));
        return;
    }

    // clear the buffering state
    SetBuffering(false);

    RefreshPauseFrame();
    PreProcessNormalFrame(); // Allow interactiveTV to draw on pause frame

    FrameScanType scan = GetScanType();
    scan = (kScan_Detect == scan || kScan_Ignore == scan) ? kScan_Progressive : scan;
    RenderVideoFrame(nullptr, scan, true, 0ms);
}

bool MythPlayerUI::DisplayNormalFrame(bool CheckPrebuffer)
{
    if (m_allPaused)
        return false;

    if (CheckPrebuffer && !PrebufferEnoughFrames())
        return false;

    // clear the buffering state
    SetBuffering(false);

    // retrieve the next frame
    m_videoOutput->StartDisplayingFrame();
    MythVideoFrame *frame = m_videoOutput->GetLastShownFrame();

    // Check aspect ratio
    CheckAspectRatio(frame);

    if (m_decoder && m_fpsMultiplier != m_decoder->GetfpsMultiplier())
        UpdateFFRewSkip();

    // Player specific processing (dvd, bd, mheg etc)
    PreProcessNormalFrame();

    // handle scan type changes
    AutoDeint(frame, m_videoOutput, m_frameInterval);

    // Detect letter boxing
    // FIXME this is temporarily moved entirely into the main thread as there are
    // too many threading issues and the decoder thread is not yet ready to handle
    // QObject signals and slots.
    // When refactoring is complete, move it into the decoder thread and
    // signal switches via a new field in MythVideoFrame
    AdjustFillMode current = m_videoOutput->GetAdjustFill();
    if (m_detectLetterBox.Detect(frame, m_videoAspect, current))
    {
        m_videoOutput->ToggleAdjustFill(current);
        ReinitOSD();
    }

    // When is the next frame due
    std::chrono::microseconds due = m_avSync.AVSync(&m_audio, frame, m_frameInterval, m_playSpeed, !m_videoDim.isEmpty(),
                                  !m_normalSpeed || FlagIsSet(kMusicChoice));
    // Display it
    DoDisplayVideoFrame(frame, due);
    m_videoOutput->DoneDisplayingFrame(frame);
    m_outputJmeter.RecordCycleTime();

    return true;
}

void MythPlayerUI::SetVideoParams(int Width, int Height, double FrameRate, float Aspect,
                                         bool ForceUpdate, int ReferenceFrames,
                                         FrameScanType Scan, const QString &CodecName)
{
    MythPlayer::SetVideoParams(Width, Height, FrameRate, Aspect, ForceUpdate, ReferenceFrames,
                               Scan, CodecName);

    // ensure deinterlacers are correctly reset after a change
    UnlockScan();
    FrameScanType newscan = DetectInterlace(Scan, static_cast<float>(m_videoFrameRate), m_videoDispDim.height());
    SetScanType(newscan, m_videoOutput, m_frameInterval);
    ResetTracker();
}

/**
 *  \brief Determines if the recording should be considered watched
 *
 *   By comparing the number of framesPlayed to the total number of
 *   frames in the video minus an offset (14%) we determine if the
 *   recording is likely to have been watched to the end, ignoring
 *   end credits and trailing adverts.
 *
 *   PlaybackInfo::SetWatchedFlag is then called with the argument TRUE
 *   or FALSE accordingly.
 *
 *   \param forceWatched Forces a recording watched ignoring the amount
 *                       actually played (Optional)
 */
void MythPlayerUI::SetWatched(bool ForceWatched)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (!m_playerCtx->m_playingInfo)
    {
        m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    uint64_t numFrames = GetCurrentFrameCount();

    // For recordings we want to ignore the post-roll and account for
    // in-progress recordings where totalFrames doesn't represent
    // the full length of the recording. For videos we can only rely on
    // totalFrames as duration metadata can be wrong
    if (m_playerCtx->m_playingInfo->IsRecording() &&
        m_playerCtx->m_playingInfo->QueryTranscodeStatus() != TRANSCODING_COMPLETE)
    {

        // If the recording is stopped early we need to use the recording end
        // time, not the programme end time
        ProgramInfo* pi  = m_playerCtx->m_playingInfo;
        qint64 starttime = pi->GetRecordingStartTime().toSecsSinceEpoch();
        qint64 endactual = pi->GetRecordingEndTime().toSecsSinceEpoch();
        qint64 endsched  = pi->GetScheduledEndTime().toSecsSinceEpoch();
        qint64 endtime   = std::min(endactual, endsched);
        numFrames = static_cast<uint64_t>((endtime - starttime) * m_videoFrameRate);
    }

    // 4 minutes min, 12 minutes max
    auto offset = std::chrono::seconds(lround(0.14 * (numFrames / m_videoFrameRate)));
    offset = std::clamp(offset, 240s, 720s);

    if (ForceWatched || (m_framesPlayed > (numFrames - (offset.count() * m_videoFrameRate))))
    {
        m_playerCtx->m_playingInfo->SaveWatched(true);
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Marking recording as watched using offset %1 minutes")
            .arg(offset.count()/60));
    }

    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void MythPlayerUI::SetBookmark(bool Clear)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->SaveBookmark(Clear ? 0 : m_framesPlayed);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void MythPlayerUI::SetLastPlayPosition(uint64_t frame)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->SaveLastPlayPos(frame);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
}

bool MythPlayerUI::CanSupportDoubleRate()
{
    // At this point we may not have the correct frame rate.
    // Since interlaced is always at 25 or 30 fps, if the interval
    // is less than 30000 (33fps) it must be representing one
    // field and not one frame, so multiply by 2.
    std::chrono::microseconds realfi = m_frameInterval;
    if (m_frameInterval < 30ms)
        realfi = m_frameInterval * 2;
    return (duration_cast<floatusecs>(realfi) / 2.0) > (duration_cast<floatusecs>(m_refreshInterval) * 0.995);
}

void MythPlayerUI::GetPlaybackData(InfoMap& Map)
{
    QString samplerate = MythMediaBuffer::BitrateToString(static_cast<uint64_t>(m_audio.GetSampleRate()), true);
    Map.insert("samplerate",  samplerate);
    Map.insert("filename",    m_playerCtx->m_buffer->GetSafeFilename());
    Map.insert("decoderrate", m_playerCtx->m_buffer->GetDecoderRate());
    Map.insert("storagerate", m_playerCtx->m_buffer->GetStorageRate());
    Map.insert("bufferavail", m_playerCtx->m_buffer->GetAvailableBuffer());
    Map.insert("buffersize",  QString::number(m_playerCtx->m_buffer->GetBufferSize() >> 20));
    m_avSync.GetAVSyncData(Map);

    if (m_videoOutput)
    {
        QString frames = QString("%1/%2").arg(m_videoOutput->ValidVideoFrames())
                                         .arg(m_videoOutput->FreeVideoFrames());
        Map.insert("videoframes", frames);
    }
    if (m_decoder)
        Map["videodecoder"] = m_decoder->GetCodecDecoderName();

    Map["framerate"] = QString("%1%2%3")
            .arg(static_cast<double>(m_outputJmeter.GetLastFPS()), 0, 'f', 2).arg(QChar(0xB1, 0))
            .arg(static_cast<double>(m_outputJmeter.GetLastSD()), 0, 'f', 2);
    Map["load"] = m_outputJmeter.GetLastCPUStats();

    GetCodecDescription(Map);

    QString displayfps = QString("%1x%2@%3Hz")
        .arg(m_display->GetResolution().width())
        .arg(m_display->GetResolution().height())
        .arg(m_display->GetRefreshRate(), 0, 'f', 2);
    Map["displayfps"] = displayfps;
}

void MythPlayerUI::GetCodecDescription(InfoMap& Map)
{
    Map["audiocodec"]    = avcodec_get_name(m_audio.GetCodec());
    Map["audiochannels"] = QString::number(m_audio.GetOrigChannels());

    int width  = m_videoDispDim.width();
    int height = m_videoDispDim.height();
    Map["videocodec"]     = GetEncodingType();
    if (m_decoder)
        Map["videocodecdesc"] = m_decoder->GetRawEncodingType();
    Map["videowidth"]     = QString::number(width);
    Map["videoheight"]    = QString::number(height);
    Map["videoframerate"] = QString::number(m_videoFrameRate, 'f', 2);
    Map["deinterlacer"]   = GetDeinterlacerName();

    if (width < 640)
        return;

    bool interlaced = is_interlaced(GetScanType());
    if (height > 2100)
        Map["videodescrip"] = interlaced ? "UHD_4K_I" : "UHD_4K_P";
    else if (width == 1920 || height == 1080 || height == 1088)
        Map["videodescrip"] = interlaced ? "HD_1080_I" : "HD_1080_P";
    else if ((width == 1280 || height == 720) && !interlaced)
        Map["videodescrip"] = "HD_720_P";
    else if (height >= 720)
        Map["videodescrip"] = "HD";
    else
        Map["videodescrip"] = "SD";
}

void MythPlayerUI::OSDDebugVisibilityChanged(bool Visible)
{
    m_osdDebug = Visible;
    if (Visible)
    {
        // This should already have enabled the required monitors
        m_osdDebugTimer.start();
    }
    else
    {
        // If we have cleared/escaped the debug OSD screen, then ChangeOSDDebug
        // is not called, so we need to ensure we turn off the monitors
        m_osdDebugTimer.stop();
        EnableBitrateMonitor();
        EnableFrameRateMonitor();
    }
}

void MythPlayerUI::UpdateOSDDebug()
{
    m_osdLock.lock();
    InfoMap infoMap;
    GetPlaybackData(infoMap);
    m_osd.ResetWindow(OSD_WIN_DEBUG);
    m_osd.SetText(OSD_WIN_DEBUG, infoMap, kOSDTimeout_None);
    m_osdLock.unlock();
}

void MythPlayerUI::ChangeOSDDebug()
{
    m_osdLock.lock();
    bool enable = !m_osdDebug;
    EnableBitrateMonitor(enable);
    EnableFrameRateMonitor(enable);
    if (enable)
        UpdateOSDDebug();
    else
        m_osd.HideWindow(OSD_WIN_DEBUG);
    m_osdDebug = enable;
    m_osdLock.unlock();
}

void MythPlayerUI::EnableFrameRateMonitor(bool Enable)
{
    bool enable = VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_ANY) || Enable;
    m_outputJmeter.SetNumCycles(enable ? static_cast<int>(m_videoFrameRate) : 0);
}

void MythPlayerUI::EnableBitrateMonitor(bool Enable)
{
    if (m_playerCtx->m_buffer)
        m_playerCtx->m_buffer->EnableBitrateMonitor(Enable);
}

/* JumpToStream, JumpToProgram and SwitchToProgram all need to be moved into the
 * parent object and hopefully simplified. The fairly involved logic does not
 * sit well with the new design objectives for the player classes and are better
 * suited to the high level logic in TV.
 */
void MythPlayerUI::JumpToStream(const QString &stream)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToStream - begin");

    // Shouldn't happen
    if (stream.isEmpty())
        return;

    Pause();
    ResetCaptions();

    ProgramInfo pginfo(stream);
    SetPlayingInfo(pginfo);

    if (m_playerCtx->m_buffer->GetType() != kMythBufferMHEG)
        m_playerCtx->m_buffer = new MythInteractiveBuffer(stream, m_playerCtx->m_buffer);
    else
        m_playerCtx->m_buffer->OpenFile(stream);

    if (!m_playerCtx->m_buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToStream buffer OpenFile failed");
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening remote stream buffer"));
        return;
    }

    m_watchingRecording = false;
    m_totalLength = 0s;
    m_totalFrames = 0;
    m_totalDuration = 0s;

    // 120 retries ~= 60 seconds
    if (OpenFile(120) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToStream OpenFile failed.");
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening remote stream"));
        return;
    }

    if (m_totalLength == 0s)
    {
        long long len = m_playerCtx->m_buffer->GetRealFileSize();
        m_totalLength = std::chrono::seconds(len / ((m_decoder->GetRawBitrate() * 1000) / 8));
        m_totalFrames = static_cast<uint64_t>(m_totalLength.count() * SafeFPS());
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("JumpToStream length %1 bytes @ %2 Kbps = %3 Secs, %4 frames @ %5 fps")
        .arg(m_playerCtx->m_buffer->GetRealFileSize()).arg(m_decoder->GetRawBitrate())
        .arg(m_totalLength.count()).arg(m_totalFrames).arg(m_decoder->GetFPS()) );

    SetEof(kEofStateNone);

    // the bitrate is reset by m_playerCtx->m_buffer->OpenFile()...
    m_playerCtx->m_buffer->UpdateRawBitrate(m_decoder->GetRawBitrate());
    m_decoder->SetProgramInfo(pginfo);

    Play();
    ChangeSpeed();

    m_playerCtx->SetPlayerChangingBuffers(false);
#ifdef USING_MHEG
    if (m_interactiveTV) m_interactiveTV->StreamStarted();
#endif

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToStream - end");
}

void MythPlayerUI::SwitchToProgram()
{
    if (!IsReallyNearEnd())
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "SwitchToProgram - start");
    bool discontinuity = false;
    bool newtype = false;
    int newid = -1;
    ProgramInfo *pginfo = m_playerCtx->m_tvchain->GetSwitchProgram(discontinuity, newtype, newid);
    if (!pginfo)
        return;

    bool newIsDummy = m_playerCtx->m_tvchain->GetInputType(newid) == "DUMMY";

    SetPlayingInfo(*pginfo);
    Pause();
    ChangeSpeed();

    // Release all frames to ensure the current decoder resources are released
    DiscardVideoFrames(true, true);

    if (newIsDummy)
    {
        OpenDummy();
        ResetPlaying();
        SetEof(kEofStateNone);
        delete pginfo;
        return;
    }

    if (m_playerCtx->m_buffer->GetType() == kMythBufferMHEG)
    {
        // Restore original ringbuffer
        auto *ic = dynamic_cast<MythInteractiveBuffer*>(m_playerCtx->m_buffer);
        if (ic)
            m_playerCtx->m_buffer = ic->TakeBuffer();
        delete ic;
    }

    m_playerCtx->m_buffer->OpenFile(pginfo->GetPlaybackURL(), MythMediaBuffer::kLiveTVOpenTimeout);

    if (!m_playerCtx->m_buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("SwitchToProgram's OpenFile failed (input type: %1)")
            .arg(m_playerCtx->m_tvchain->GetInputType(newid)));
        LOG(VB_GENERAL, LOG_ERR, m_playerCtx->m_tvchain->toString());
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening switch program buffer"));
        delete pginfo;
        return;
    }

    if (GetEof() != kEofStateNone)
    {
        discontinuity = true;
        ResetCaptions();
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SwitchToProgram(void) "
        "discont: %1 newtype: %2 newid: %3 decoderEof: %4")
        .arg(discontinuity).arg(newtype).arg(newid).arg(GetEof()));

    if (discontinuity || newtype)
    {
        m_playerCtx->m_tvchain->SetProgram(*pginfo);
        if (m_decoder)
            m_decoder->SetProgramInfo(*pginfo);

        m_playerCtx->m_buffer->Reset(true);
        if (newtype)
        {
            if (OpenFile() < 0)
                SetErrored(tr("Error opening switch program file"));
        }
        else
        {
            ResetPlaying();
        }
    }
    else
    {
        m_playerCtx->SetPlayerChangingBuffers(true);
        if (m_decoder)
        {
            m_decoder->SetReadAdjust(m_playerCtx->m_buffer->SetAdjustFilesize());
            m_decoder->SetWaitForChange();
        }
    }
    delete pginfo;

    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SwitchToProgram failed.");
        SetEof(kEofStateDelayed);
        return;
    }

    SetEof(kEofStateNone);

    // the bitrate is reset by m_playerCtx->m_buffer->OpenFile()...
    if (m_decoder)
        m_playerCtx->m_buffer->UpdateRawBitrate(m_decoder->GetRawBitrate());
    m_playerCtx->m_buffer->Unpause();

    if (discontinuity || newtype)
    {
        CheckTVChain();
        m_forcePositionMapSync = true;
    }

    Play();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "SwitchToProgram - end");
}

void MythPlayerUI::JumpToProgram()
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToProgram - start");
    bool discontinuity = false;
    bool newtype = false;
    int newid = -1;
    std::chrono::seconds nextpos = m_playerCtx->m_tvchain->GetJumpPos();
    ProgramInfo *pginfo = m_playerCtx->m_tvchain->GetSwitchProgram(discontinuity, newtype, newid);
    if (!pginfo)
        return;

    m_inJumpToProgramPause = true;

    bool newIsDummy = m_playerCtx->m_tvchain->GetInputType(newid) == "DUMMY";
    SetPlayingInfo(*pginfo);

    Pause();
    ChangeSpeed();
    ResetCaptions();

    // Release all frames to ensure the current decoder resources are released
    DiscardVideoFrames(true, true);

    m_playerCtx->m_tvchain->SetProgram(*pginfo);
    m_playerCtx->m_buffer->Reset(true);

    if (newIsDummy)
    {
        OpenDummy();
        ResetPlaying();
        SetEof(kEofStateNone);
        delete pginfo;
        m_inJumpToProgramPause = false;
        return;
    }

    SendMythSystemPlayEvent("PLAY_CHANGED", pginfo);

    if (m_playerCtx->m_buffer->GetType() == kMythBufferMHEG)
    {
        // Restore original ringbuffer
        auto *ic = dynamic_cast<MythInteractiveBuffer*>(m_playerCtx->m_buffer);
        if (ic)
            m_playerCtx->m_buffer = ic->TakeBuffer();
        delete ic;
    }

    m_playerCtx->m_buffer->OpenFile(pginfo->GetPlaybackURL(), MythMediaBuffer::kLiveTVOpenTimeout);
    if (!m_playerCtx->m_buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("JumpToProgram's OpenFile failed (input type: %1)")
                .arg(m_playerCtx->m_tvchain->GetInputType(newid)));
        LOG(VB_GENERAL, LOG_ERR, m_playerCtx->m_tvchain->toString());
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening jump program file buffer"));
        delete pginfo;
        m_inJumpToProgramPause = false;
        return;
    }

    LoadExternalSubtitles();

    bool wasDummy = m_isDummy;
    if (newtype || wasDummy)
    {
        if (OpenFile() < 0)
            SetErrored(tr("Error opening jump program file"));
    }
    else
    {
        ResetPlaying();
    }

    if (IsErrored() || !m_decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToProgram failed.");
        if (!IsErrored())
            SetErrored(tr("Error reopening video decoder"));
        delete pginfo;
        m_inJumpToProgramPause = false;
        return;
    }

    SetEof(kEofStateNone);

    // the bitrate is reset by m_playerCtx->m_buffer->OpenFile()...
    m_playerCtx->m_buffer->UpdateRawBitrate(m_decoder->GetRawBitrate());
    m_playerCtx->m_buffer->IgnoreLiveEOF(false);

    m_decoder->SetProgramInfo(*pginfo);
    delete pginfo;

    CheckTVChain();
    m_forcePositionMapSync = true;
    m_inJumpToProgramPause = false;
    Play();
    ChangeSpeed();

    // check that we aren't too close to the end of program.
    // and if so set it to 10s from the end if completed recordings
    // or 3s if live
    std::chrono::seconds duration = m_playerCtx->m_tvchain->GetLengthAtCurPos();
    std::chrono::seconds maxpos = m_playerCtx->m_tvchain->HasNext() ? 10s : 3s;

    if (nextpos > (duration - maxpos))
    {
        nextpos = duration - maxpos;
        if (nextpos < 0s)
            nextpos = 0s;
    }
    else if (nextpos < 0s)
    {
        // it's a relative position to the end
        nextpos += duration;
    }

    // nextpos is the new position to use in seconds
    uint64_t nextframe = TranslatePositionMsToFrame(nextpos, true);

    if (nextpos > 10s)
        DoJumpToFrame(nextframe, kInaccuracyNone);

    m_playerCtx->SetPlayerChangingBuffers(false);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToProgram - end");
}
