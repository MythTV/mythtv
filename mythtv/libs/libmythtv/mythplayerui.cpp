// MythTV
#include "audiooutput.h"
#include "mythmainwindow.h"
#include "interactivetv.h"
#include "osd.h"
#include "tv_play.h"
#include "livetvchain.h"
#include "mythvideooutgpu.h"
#include "mythplayerui.h"

#define LOC QString("PlayerUI: ")

MythPlayerUI::MythPlayerUI(MythMainWindow* MainWindow, TV* Tv,
                                         PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerVisualiserUI(MainWindow, Tv, Context, Flags),
    MythVideoScanTracker(this)
{
}

void MythPlayerUI::EventLoop()
{
    // Handle decoder callbacks
    ProcessCallbacks();

    // Live TV program change
    if (m_fileChanged)
        FileChanged();

    // recreate the osd if a reinit was triggered by another thread
    if (m_reinitOsd)
        ReinitOSD();

    // reselect subtitle tracks if triggered by the decoder
    if (m_enableCaptions)
        SetCaptionsEnabled(true, false);
    if (m_disableCaptions)
        SetCaptionsEnabled(false, false);

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
            m_deleteMap.UpdateOSD(m_framesPlayed, m_videoFrameRate, m_osd);
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
        m_playerCtx->m_tvchain->JumpToNext(true, 0);
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
    { QMutexLocker locker(&m_streamLock);
    if (!m_newStream.isEmpty())
    {
        QString stream = m_newStream;
        m_newStream.clear();
        locker.unlock();
        JumpToStream(stream);
    }}

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
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Unknown recorder error, exiting decoder");
        if (!IsErrored())
            SetErrored(tr("Irrecoverable recorder error"));
        m_killDecoder = true;
        return;
    }

    // Handle speed change
    if (!qFuzzyCompare(m_playSpeed + 1.0F, m_nextPlaySpeed + 1.0F) &&
        (!m_playerCtx->m_tvchain || (m_playerCtx->m_tvchain && !m_playerCtx->m_tvchain->NeedsToJump())))
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
            m_playerCtx->m_tvchain->JumpToNext(true, 0);
            return;
        }

        bool videoDrained =
            m_videoOutput && m_videoOutput->ValidVideoFrames() < 1;
        bool audioDrained =
            !m_audio.GetAudioOutput() ||
            m_audio.IsPaused() ||
            m_audio.GetAudioOutput()->GetAudioBufferedTime() < 100;
        if (eof != kEofStateDelayed || (videoDrained && audioDrained))
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
            if (!(m_endExitPrompt == 1 && m_playerCtx->GetState() == kState_WatchingPreRecorded))
                SetEof(kEofStateDelayed);
        }
        else
        {
            DoJumpToFrame(jumpto, kInaccuracyNone);
        }
    }
}

void MythPlayerUI::ChangeSpeed()
{
    MythPlayer::ChangeSpeed();
    // ensure we re-check double rate support following a speed change
    UnlockScan();
}

bool MythPlayerUI::InitVideo()
{
    if (!(m_playerCtx && m_decoder))
        return false;

    m_videoOutput = MythVideoOutputGPU::Create(m_mainWindow,
                    m_decoder->GetCodecDecoderName(), m_decoder->GetVideoCodecID(),
                    m_videoDim, m_videoDispDim, m_videoAspect,
                    static_cast<float>(m_videoFrameRate),
                    static_cast<uint>(m_playerFlags), m_codecName, m_maxReferenceFrames);

    if (m_videoOutput)
    {
        connect(m_tv, &TV::EmbedPlayback, m_videoOutput, &MythVideoBounds::EmbedPlayback);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't create VideoOutput instance. Exiting..");
    SetErrored(tr("Failed to initialize video output"));
    return false;
}

void MythPlayerUI::ReinitVideo(bool ForceUpdate)
{
    MythPlayer::ReinitVideo(ForceUpdate);
    AutoVisualise(!m_videoDim.isEmpty());
}

void MythPlayerUI::VideoStart()
{
    QRect visible;
    QRect total;
    float aspect = NAN;
    float scaling = NAN;

    m_osdLock.lock();
    m_osd = new OSD(this, m_tv, m_videoOutput->GetOSDPainter());
    m_videoOutput->GetOSDBounds(total, visible, aspect, scaling, 1.0F);
    m_osd->Init(visible, aspect);
    m_osd->EnableSubtitles(kDisplayNone);

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
        SetTrack(kTrackTypeRawText, static_cast<int>(forcedTrackNumber));
    else
        SetCaptionsEnabled(m_captionsEnabledbyDefault, false);

    m_osdLock.unlock();

    MythPlayer::VideoStart();
    InitialiseScan(m_videoOutput);
    EnableFrameRateMonitor();
    AutoVisualise(!m_videoDim.isEmpty());
}

bool MythPlayerUI::VideoLoop()
{
    ProcessCallbacks();

    if (m_videoPaused || m_isDummy)
        DisplayPauseFrame();
    else
        DisplayNormalFrame();

    if (FlagIsSet(kVideoIsNull) && m_decoder)
        m_decoder->UpdateFramesPlayed();
    else if (m_decoder && m_decoder->GetEof() != kEofStateNone)
        ++m_framesPlayed;
    else
        m_framesPlayed = static_cast<uint64_t>(m_videoOutput->GetFramesPlayed());
    return !IsErrored();
}

void MythPlayerUI::InitFrameInterval()
{
    SetFrameInterval(GetScanType(), 1.0 / (m_videoFrameRate * static_cast<double>(m_playSpeed)));
    MythPlayer::InitFrameInterval();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Display Refresh Rate: %1 Video Frame Rate: %2")
        .arg(1000000.0 / m_display->GetRefreshInterval(m_frameInterval), 0, 'f', 3)
        .arg(1000000.0 / m_frameInterval, 0, 'f', 3));
}

void MythPlayerUI::RenderVideoFrame(MythVideoFrame *Frame, FrameScanType Scan, bool Prepare, int64_t Wait)
{
    if (!m_videoOutput)
        return;

    if (Prepare)
        m_videoOutput->PrepareFrame(Frame, Scan);
    PrepareVisualiser();
    m_videoOutput->RenderFrame(Frame, Scan);
    RenderVisualiser();
    m_osdLock.lock();
    m_videoOutput->RenderOverlays(m_osd);
    m_osdLock.unlock();
    m_videoOutput->RenderEnd();

    if (Wait > 0)
        m_avSync.WaitForFrame(Wait);

    m_videoOutput->EndFrame();
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
                if (m_osd)
                    DeleteMap::UpdateOSD(GetLatestVideoTimecode(), m_osd);
                m_osdLock.unlock();
            }
        }
        else
        {
            m_decodeOneFrame = true;
        }
    }
}

void MythPlayerUI::DoDisplayVideoFrame(MythVideoFrame* Frame, int64_t Due)
{
    if (Due < 0)
    {
        m_videoOutput->SetFramesPlayed(static_cast<long long>(++m_framesPlayed));
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
    RenderVideoFrame(nullptr, scan, true, 0);
}

void MythPlayerUI::DisplayNormalFrame(bool CheckPrebuffer)
{
    if (m_allPaused)
        return;

    if (CheckPrebuffer && !PrebufferEnoughFrames())
        return;

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
    m_detectLetterBox.SwitchTo(frame);

    // When is the next frame due
    int64_t due = m_avSync.AVSync(&m_audio, frame, m_frameInterval, m_playSpeed, !m_videoDim.isEmpty(),
                                  !m_normalSpeed || FlagIsSet(kMusicChoice));
    // Display it
    DoDisplayVideoFrame(frame, due);
    m_videoOutput->DoneDisplayingFrame(frame);
    m_outputJmeter.RecordCycleTime();
}

void MythPlayerUI::ReleaseNextVideoFrame(MythVideoFrame* Frame, int64_t Timecode, bool Wrap)
{
    MythPlayer::ReleaseNextVideoFrame(Frame, Timecode, Wrap);
    m_detectLetterBox.Detect(Frame);
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

void MythPlayerUI::WindowResized(const QSize& Size)
{
    if (m_videoOutput)
        m_videoOutput->WindowResized(Size);
    ReinitOSD();
}

bool MythPlayerUI::CanSupportDoubleRate()
{
    int refreshinterval = 1;
    if (m_display)
        refreshinterval = m_display->GetRefreshInterval(m_frameInterval);

    // At this point we may not have the correct frame rate.
    // Since interlaced is always at 25 or 30 fps, if the interval
    // is less than 30000 (33fps) it must be representing one
    // field and not one frame, so multiply by 2.
    int realfi = m_frameInterval;
    if (m_frameInterval < 30000)
        realfi = m_frameInterval * 2;
    return ((realfi / 2.0) > (refreshinterval * 0.995));
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
}

void MythPlayerUI::GetCodecDescription(InfoMap& Map)
{
    Map["audiocodec"]    = ff_codec_id_string(m_audio.GetCodec());
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

void MythPlayerUI::EnableFrameRateMonitor(bool Enable)
{
    bool verbose = VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_ANY);
    double rate = Enable ? m_videoFrameRate : verbose ? (m_videoFrameRate * 4) : 0.0;
    m_outputJmeter.SetNumCycles(static_cast<int>(rate));
}

void MythPlayerUI::ToggleAdjustFill(AdjustFillMode Mode)
{
    if (m_videoOutput)
    {
        m_detectLetterBox.SetDetectLetterbox(false);
        m_videoOutput->ToggleAdjustFill(Mode);
        ReinitOSD();
    }
}

// Only edit stuff below here - to be moved
#include "tv_actions.h"

uint64_t MythPlayerUI::GetNearestMark(uint64_t Frame, bool Right)
{
    return m_deleteMap.GetNearestMark(Frame, Right);
}

bool MythPlayerUI::IsTemporaryMark(uint64_t Frame)
{
    return m_deleteMap.IsTemporaryMark(Frame);
}

bool MythPlayerUI::HasTemporaryMark()
{
    return m_deleteMap.HasTemporaryMark();
}

bool MythPlayerUI::IsCutListSaved()
{
    return m_deleteMap.IsSaved();
}

bool MythPlayerUI::DeleteMapHasUndo()
{
    return m_deleteMap.HasUndo();
}

bool MythPlayerUI::DeleteMapHasRedo()
{
    return m_deleteMap.HasRedo();
}

QString MythPlayerUI::DeleteMapGetUndoMessage()
{
    return m_deleteMap.GetUndoMessage();
}

QString MythPlayerUI::DeleteMapGetRedoMessage()
{
    return m_deleteMap.GetRedoMessage();
}

bool MythPlayerUI::EnableEdit()
{
    m_deleteMap.SetEditing(false);

    if (!m_hasFullPositionMap)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot edit - no full position map");
        SetOSDStatus(tr("No Seektable"), kOSDTimeout_Med);
        return false;
    }

    if (m_deleteMap.IsFileEditing())
        return false;

    QMutexLocker locker(&m_osdLock);
    if (!m_osd)
        return false;

    SetupAudioGraph(m_videoFrameRate);

    m_savedAudioTimecodeOffset = m_tcWrap[TC_AUDIO];
    m_tcWrap[TC_AUDIO] = 0;

    m_speedBeforeEdit = m_playSpeed;
    m_pausedBeforeEdit = Pause();
    m_deleteMap.SetEditing(true);
    m_osd->DialogQuit();
    ResetCaptions();
    m_osd->HideAll();

    bool loadedAutoSave = m_deleteMap.LoadAutoSaveMap();
    if (loadedAutoSave)
        SetOSDMessage(tr("Using previously auto-saved cuts"), kOSDTimeout_Short);

    m_deleteMap.UpdateSeekAmount(0);
    m_deleteMap.UpdateOSD(m_framesPlayed, m_videoFrameRate, m_osd);
    m_deleteMap.SetFileEditing(true);
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->SaveEditing(true);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    m_editUpdateTimer.start();

    return m_deleteMap.IsEditing();
}

/*! \brief Leave cutlist edit mode, saving work in 1 of 3 ways.
 *
 *  \param HowToSave If 1, save all changes.  If 0, discard all
 *  changes.  If -1, do not explicitly save changes but leave
 *  auto-save information intact in the database.
 */
void MythPlayerUI::DisableEdit(int HowToSave)
{
    QMutexLocker locker(&m_osdLock);
    if (!m_osd)
        return;

    m_deleteMap.SetEditing(false, m_osd);
    if (HowToSave == 0)
        m_deleteMap.LoadMap();
    // Unconditionally save to remove temporary marks from the DB.
    if (HowToSave >= 0)
        m_deleteMap.SaveMap();
    m_deleteMap.TrackerReset(m_framesPlayed);
    m_deleteMap.SetFileEditing(false);
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->SaveEditing(false);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    ClearAudioGraph();
    m_tcWrap[TC_AUDIO] = m_savedAudioTimecodeOffset;
    m_savedAudioTimecodeOffset = 0;

    if (!m_pausedBeforeEdit)
        Play(m_speedBeforeEdit);
    else
        SetOSDStatus(tr("Paused"), kOSDTimeout_None);
}

bool MythPlayerUI::HandleProgramEditorActions(QStringList& Actions)
{
    bool handled = false;
    bool refresh = true;
    auto frame   = GetFramesPlayed();

    for (int i = 0; i < Actions.size() && !handled; i++)
    {
        QString action = Actions[i];
        handled = true;
        float seekamount = m_deleteMap.GetSeekAmount();
        bool  seekzero   = qFuzzyCompare(seekamount + 1.0F, 1.0F);
        if (action == ACTION_LEFT)
        {
            if (seekzero) // 1 frame
            {
                DoRewind(1, kInaccuracyNone);
            }
            else if (seekamount > 0)
            {
                // Use fully-accurate seeks for less than 1 second.
                DoRewindSecs(seekamount, seekamount < 1.0F ? kInaccuracyNone :
                             kInaccuracyEditor, false);
            }
            else
            {
                HandleArbSeek(false);
            }
        }
        else if (action == ACTION_RIGHT)
        {
            if (seekzero) // 1 frame
            {
                DoFastForward(1, kInaccuracyNone);
            }
            else if (seekamount > 0)
            {
                // Use fully-accurate seeks for less than 1 second.
                DoFastForwardSecs(seekamount, seekamount < 1.0F ? kInaccuracyNone :
                             kInaccuracyEditor, false);
            }
            else
            {
                HandleArbSeek(true);
            }
        }
        else if (action == ACTION_LOADCOMMSKIP)
        {
            if (m_commBreakMap.HasMap())
            {
                frm_dir_map_t map;
                m_commBreakMap.GetMap(map);
                m_deleteMap.LoadCommBreakMap(map);
            }
        }
        else if (action == ACTION_PREVCUT)
        {
            float old_seekamount = m_deleteMap.GetSeekAmount();
            m_deleteMap.SetSeekAmount(-2);
            HandleArbSeek(false);
            m_deleteMap.SetSeekAmount(old_seekamount);
        }
        else if (action == ACTION_NEXTCUT)
        {
            float old_seekamount = m_deleteMap.GetSeekAmount();
            m_deleteMap.SetSeekAmount(-2);
            HandleArbSeek(true);
            m_deleteMap.SetSeekAmount(old_seekamount);
        }
#define FFREW_MULTICOUNT 10.0F
        else if (action == ACTION_BIGJUMPREW)
        {
            if (seekzero)
                DoRewind(FFREW_MULTICOUNT, kInaccuracyNone);
            else if (seekamount > 0)
                DoRewindSecs(seekamount * FFREW_MULTICOUNT, kInaccuracyEditor, false);
            else
                DoRewindSecs(FFREW_MULTICOUNT / 2, kInaccuracyNone, false);
        }
        else if (action == ACTION_BIGJUMPFWD)
        {
            if (seekzero)
                DoFastForward(FFREW_MULTICOUNT, kInaccuracyNone);
            else if (seekamount > 0)
                DoFastForwardSecs(seekamount * FFREW_MULTICOUNT, kInaccuracyEditor, false);
            else
                DoFastForwardSecs(FFREW_MULTICOUNT / 2, kInaccuracyNone, false);
        }
        else if (action == ACTION_SELECT)
        {
            m_deleteMap.NewCut(frame);
            SetOSDMessage(tr("New cut added."), kOSDTimeout_Short);
            refresh = true;
        }
        else if (action == "DELETE")
        {
            m_deleteMap.Delete(frame, tr("Delete"));
            refresh = true;
        }
        else if (action == "REVERT")
        {
            m_deleteMap.LoadMap(tr("Undo Changes"));
            refresh = true;
        }
        else if (action == "REVERTEXIT")
        {
            DisableEdit(0);
            refresh = false;
        }
        else if (action == ACTION_SAVEMAP)
        {
            m_deleteMap.SaveMap();
            refresh = true;
        }
        else if (action == "EDIT" || action == "SAVEEXIT")
        {
            DisableEdit(1);
            refresh = false;
        }
        else
        {
            QString undoMessage = m_deleteMap.GetUndoMessage();
            QString redoMessage = m_deleteMap.GetRedoMessage();
            handled = m_deleteMap.HandleAction(action, frame);
            if (handled && (action == "CUTTOBEGINNING" || action == "CUTTOEND" || action == "NEWCUT"))
                SetOSDMessage(tr("New cut added."), kOSDTimeout_Short);
            else if (handled && action == "UNDO")
                SetOSDMessage(tr("Undo - %1").arg(undoMessage), kOSDTimeout_Short);
            else if (handled && action == "REDO")
                SetOSDMessage(tr("Redo - %1").arg(redoMessage), kOSDTimeout_Short);
        }
    }

    if (handled && refresh)
    {
        m_osdLock.lock();
        if (m_osd)
            m_deleteMap.UpdateOSD(m_framesPlayed, m_videoFrameRate, m_osd);
        m_osdLock.unlock();
    }

    return handled;
}

