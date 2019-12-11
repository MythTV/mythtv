// MythTV
#include "dvdringbuffer.h"
#include "DetectLetterbox.h"
#include "audiooutput.h"
#include "avformatdecoderdvd.h"
#include "mythdvdplayer.h"

// Std
#include <unistd.h> // for usleep()

#define LOC      QString("DVDPlayer: ")

void MythDVDPlayer::AutoDeint(VideoFrame *Frame, bool AllowLock)
{
    bool dummy = false;
    if (decoder && decoder->GetMythCodecContext()->IsDeinterlacing(dummy))
    {
        MythPlayer::AutoDeint(Frame, AllowLock);
        return;
    }

    SetScanType(kScan_Interlaced);
}

/** \fn MythDVDPlayer::ReleaseNextVideoFrame(VideoFrame*, int64_t)
 *  \param buffer    Buffer
 *  \param timecode  Timecode
 *  \param wrap      Ignored. This function overrides the callers
 *                   'wrap' indication and computes its own based on
 *                   whether or not video is currently playing.
 */
void MythDVDPlayer::ReleaseNextVideoFrame(VideoFrame *Buffer,
                                          int64_t Timecode, bool /*wrap*/)
{
    MythPlayer::ReleaseNextVideoFrame(Buffer, Timecode,
                        !player_ctx->m_buffer->IsInDiscMenuOrStillFrame());
}

bool MythDVDPlayer::HasReachedEof(void) const
{
    EofState eof = GetEof();
    // DeleteMap and EditMode from the parent MythPlayer should not be
    // relevant here.
    return eof != kEofStateNone && !allpaused;
}

void MythDVDPlayer::DisableCaptions(uint Mode, bool OSDMsg)
{
    if ((kDisplayAVSubtitle & Mode) && player_ctx->m_buffer->IsDVD())
        player_ctx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle, -1);
    MythPlayer::DisableCaptions(Mode, OSDMsg);
}

void MythDVDPlayer::EnableCaptions(uint Mode, bool OSDMsg)
{
    if ((kDisplayAVSubtitle & Mode) && player_ctx->m_buffer->IsDVD())
    {
        int track = GetTrack(kTrackTypeSubtitle);
        if (track >= 0 && track < static_cast<int>(decoder->GetTrackCount(kTrackTypeSubtitle)))
        {
            StreamInfo stream = decoder->GetTrackInfo(kTrackTypeSubtitle,
                                                      static_cast<uint>(track));
            player_ctx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle,
                                                stream.m_stream_id);
        }
    }
    MythPlayer::EnableCaptions(Mode, OSDMsg);
}

void MythDVDPlayer::DisplayPauseFrame(void)
{
    if (player_ctx->m_buffer->IsDVD() &&
        player_ctx->m_buffer->DVD()->IsInStillFrame())
    {
        SetScanType(kScan_Progressive);
    }
    DisplayDVDButton();
    MythPlayer::DisplayPauseFrame();
}

void MythDVDPlayer::DecoderPauseCheck(void)
{
    StillFrameCheck();
    MythPlayer::DecoderPauseCheck();
}

bool MythDVDPlayer::PrebufferEnoughFrames(int /*MinBuffers*/)
{
    return MythPlayer::PrebufferEnoughFrames(1);
}

bool MythDVDPlayer::DecoderGetFrameFFREW(void)
{
    bool res = MythPlayer::DecoderGetFrameFFREW();
    if (decoder_change_lock.tryLock(1))
    {
        if (decoder)
            decoder->UpdateFramesPlayed();
        decoder_change_lock.unlock();
    }
    return res;
}

bool MythDVDPlayer::DecoderGetFrameREW(void)
{
    MythPlayer::DecoderGetFrameREW();
    return (player_ctx->m_buffer->IsDVD() &&
            (player_ctx->m_buffer->DVD()->GetCurrentTime() < 2));
}

void MythDVDPlayer::PreProcessNormalFrame(void)
{
    DisplayDVDButton();
}

void MythDVDPlayer::VideoStart(void)
{
    if (!m_initial_dvdstate.isEmpty())
        player_ctx->m_buffer->DVD()->RestoreDVDStateSnapshot(m_initial_dvdstate);

    MythPlayer::VideoStart();
}

bool MythDVDPlayer::VideoLoop(void)
{
    if (!player_ctx->m_buffer->IsDVD())
    {
        SetErrored("RingBuffer is not a DVD.");
        return !IsErrored();
    }

    int nbframes = 0;
    if (videoOutput)
        nbframes = videoOutput->ValidVideoFrames();

#if 0
    LOG(VB_PLAYBACK, LOG_DEBUG,
        LOC + QString("Validframes %1, FreeFrames %2, VideoPaused %3")
           .arg(nbframes).arg(videoOutput->FreeVideoFrames()).arg(videoPaused));
#endif

    // completely drain the video buffers for certain situations
    bool release_all = player_ctx->m_buffer->DVD()->DVDWaitingForPlayer() &&
                      (nbframes > 0);
    bool release_one = (nbframes > 1) && videoPaused && !allpaused &&
                       (!videoOutput->EnoughFreeFrames() ||
                        player_ctx->m_buffer->DVD()->IsWaiting() ||
                        player_ctx->m_buffer->DVD()->IsInStillFrame());
    if (release_all || release_one)
    {
        if (nbframes < 5 && videoOutput)
            videoOutput->UpdatePauseFrame(disp_timecode);

        // if we go below the pre-buffering limit, the player will pause
        // so do this 'manually'
        DisplayNormalFrame(false);
        // unpause the still frame if more frames become available
        if (m_dvd_stillframe_showing && nbframes > 1)
        {
            m_dvd_stillframe_showing = false;
            UnpauseVideo();
        }
        return !IsErrored();
    }

    // clear the mythtv imposed wait state
    if (player_ctx->m_buffer->DVD()->DVDWaitingForPlayer())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing MythTV DVD wait state");
        player_ctx->m_buffer->DVD()->SkipDVDWaitingForPlayer();
        ClearAfterSeek(true);
        if (videoPaused && !allpaused)
            UnpauseVideo();
        return !IsErrored();
    }

    // wait for the video buffers to drain
    if (nbframes < 2)
    {
        // clear the DVD wait state
        if (player_ctx->m_buffer->DVD()->IsWaiting())
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing DVD wait state");
            player_ctx->m_buffer->DVD()->WaitSkip();
            if (videoPaused && !allpaused)
                UnpauseVideo();
            return !IsErrored();
        }

        // the still frame is treated as a pause frame
        if (player_ctx->m_buffer->DVD()->IsStillFramePending())
        {
            // ensure we refresh the pause frame
            if (!m_dvd_stillframe_showing)
                needNewPauseFrame = true;

            // we are in a still frame so pause video output
            if (!videoPaused)
            {
                PauseVideo();
                m_dvd_stillframe_showing = true;
                return !IsErrored();
            }

            // see if the pause frame has timed out
            StillFrameCheck();

            // flag if we have no frame
            if (nbframes == 0)
            {
                LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                        "In DVD Menu: No video frames in queue");
                usleep(10000);
                return !IsErrored();
            }

            m_dvd_stillframe_showing = true;
        }
    }

    // unpause the still frame if more frames become available
    if (m_dvd_stillframe_showing && nbframes > 1)
    {
        UnpauseVideo();
        m_dvd_stillframe_showing = false;
        return !IsErrored();
    }

    return MythPlayer::VideoLoop();
}

void MythDVDPlayer::DisplayLastFrame(void)
{
    // clear the buffering state
    SetBuffering(false);

    SetScanType(kScan_Progressive);
    DisplayDVDButton();

    AVSync(nullptr);
}

bool MythDVDPlayer::FastForward(float Seconds)
{
    if (decoder)
        decoder->UpdateFramesPlayed();
    return MythPlayer::FastForward(Seconds);
}

bool MythDVDPlayer::Rewind(float Seconds)
{
    if (decoder)
        decoder->UpdateFramesPlayed();
    return MythPlayer::Rewind(Seconds);
}

bool MythDVDPlayer::JumpToFrame(uint64_t Frame)
{
    if (Frame == ~0x0ULL)
        return false;

    if (decoder)
        decoder->UpdateFramesPlayed();
    return MythPlayer::JumpToFrame(Frame);
}

void MythDVDPlayer::EventStart(void)
{
    if (player_ctx->m_buffer->DVD())
        player_ctx->m_buffer->DVD()->SetParent(this);

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->m_playingInfo)
    {
        QString name;
        QString serialid;
        if (player_ctx->m_playingInfo->GetTitle().isEmpty() &&
            player_ctx->m_buffer->DVD() &&
            player_ctx->m_buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            player_ctx->m_playingInfo->SetTitle(name);
        }
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    MythPlayer::EventStart();
}

void MythDVDPlayer::InitialSeek(void)
{
    player_ctx->m_buffer->IgnoreWaitStates(true);

    if (m_initial_title > -1)
        player_ctx->m_buffer->DVD()->PlayTitleAndPart(m_initial_title, 1);

    if (m_initial_audio_track > -1)
        player_ctx->m_buffer->DVD()->SetTrack(kTrackTypeAudio,
                                            m_initial_audio_track);
    if (m_initial_subtitle_track > -1)
        player_ctx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle,
                                            m_initial_subtitle_track);

    if (bookmarkseek > 30)
    {
        // we need to trigger a dvd cell change to ensure the new title length
        // is set and the position map updated accordingly
        decodeOneFrame = true;
        int count = 0;
        while (count++ < 100 && decodeOneFrame)
            usleep(50000);
    }
    MythPlayer::InitialSeek();
    player_ctx->m_buffer->IgnoreWaitStates(false);
}

void MythDVDPlayer::ResetPlaying(bool /*ResetFrames*/)
{
    MythPlayer::ResetPlaying(false);
}

void MythDVDPlayer::EventEnd(void)
{
    if (player_ctx->m_buffer->DVD())
        player_ctx->m_buffer->DVD()->SetParent(nullptr);
}

bool MythDVDPlayer::PrepareAudioSample(int64_t &Timecode)
{
    if (!player_ctx->m_buffer->IsInDiscMenuOrStillFrame())
        WrapTimecode(Timecode, TC_AUDIO);

    return player_ctx->m_buffer->IsDVD() &&
           player_ctx->m_buffer->DVD()->IsInStillFrame();
}

void MythDVDPlayer::SetBookmark(bool Clear)
{
    if (!player_ctx->m_buffer->IsDVD())
        return;

    QStringList fields;
    QString name;
    QString serialid;
    QString dvdstate;

    if (!player_ctx->m_buffer->IsInMenu() &&
        (player_ctx->m_buffer->IsBookmarkAllowed() || Clear))
    {
        if (!player_ctx->m_buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "DVD has no name and serial number. Cannot set bookmark.");
            return;
        }

        if (!Clear && !player_ctx->m_buffer->DVD()->GetDVDStateSnapshot(dvdstate))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unable to retrieve DVD state. Cannot set bookmark.");
            return;
        }

        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->m_playingInfo)
        {
            fields += serialid;
            fields += name;

            if (!Clear)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Set bookmark");
                fields += dvdstate;
            }
            else
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clear bookmark");

            ProgramInfo::SaveDVDBookmark(fields);

        }
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }
}

uint64_t MythDVDPlayer::GetBookmark(void)
{
    if (gCoreContext->IsDatabaseIgnored() || !player_ctx->m_buffer->IsDVD())
        return 0;

    QString name;
    QString serialid;
    uint64_t frames = 0;
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->m_playingInfo)
    {
        if (!player_ctx->m_buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            return 0;
        }

        QStringList dvdbookmark = player_ctx->m_playingInfo->QueryDVDBookmark(serialid);

        if (!dvdbookmark.empty())
        {
            QStringList::Iterator it = dvdbookmark.begin();

            if (dvdbookmark.count() == 1)
            {
                m_initial_dvdstate = *it;
                frames = ~0x0ULL;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Get Bookmark: bookmark found");
            }
            else
            {
                // Legacy bookmarks
                m_initial_title = (*it).toInt();
                frames = ((*++it).toLongLong() & 0xffffffffLL);
                m_initial_audio_track    = (*++it).toInt();
                m_initial_subtitle_track = (*++it).toInt();
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Get Bookmark: title %1 audiotrack %2 subtrack %3 "
                            "frame %4")
                    .arg(m_initial_title).arg(m_initial_audio_track)
                    .arg(m_initial_subtitle_track).arg(frames));
            }
        }
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    return frames;;
}

void MythDVDPlayer::ChangeSpeed(void)
{
    if (m_stillFrameLength > 0)
    {
        m_stillFrameTimerLock.lock();
        // Get the timestretched elapsed time and transform
        // it to what the unstretched value would have been
        // had we been playing with the new timestretch value
        // all along
        int elapsed = static_cast<int>((m_stillFrameTimer.elapsed() * play_speed / next_play_speed));
        m_stillFrameTimer.restart();
        m_stillFrameTimer.addMSecs(elapsed);
        m_stillFrameTimerLock.unlock();
    }

    MythPlayer::ChangeSpeed();

    if (decoder)
        decoder->UpdateFramesPlayed();
    if (player_ctx->m_buffer->IsDVD())
    {
        if (play_speed > 1.0F)
            player_ctx->m_buffer->DVD()->SetDVDSpeed(-1);
        else
            player_ctx->m_buffer->DVD()->SetDVDSpeed();
    }
}

long long MythDVDPlayer::CalcMaxFFTime(long long FastFwd, bool Setjump) const
{
    if ((totalFrames > 0) && player_ctx->m_buffer->IsDVD() &&
        player_ctx->m_buffer->DVD()->TitleTimeLeft() < 5)
        return 0;
    return MythPlayer::CalcMaxFFTime(FastFwd, Setjump);
}

int64_t MythDVDPlayer::GetSecondsPlayed(bool /*HonorCutList*/, int Divisor)
{
    if (!player_ctx->m_buffer->IsDVD())
        return 0;

    int64_t played = player_ctx->m_buffer->DVD()->GetCurrentTime();

    if (m_stillFrameLength > 0)
    {
        if (m_stillFrameLength == 255)
            played = -1;
        else
            played = static_cast<int64_t>(m_stillFrameTimer.elapsed() * play_speed / Divisor);
    }

    return played;
}

int64_t MythDVDPlayer::GetTotalSeconds(bool /*HonorCutList*/, int Divisor) const
{
    int64_t total = totalLength;

    if (m_stillFrameLength > 0)
    {
        if (m_stillFrameLength == 255)
            return -1;
        total = m_stillFrameLength;
    }

    return total * 1000 / Divisor;
}

void MythDVDPlayer::SeekForScreenGrab(uint64_t &Number, uint64_t FrameNum,
                                      bool /*Absolute*/)
{
    if (!player_ctx->m_buffer->IsDVD())
        return;
    if (GoToMenu("menu"))
    {
        if (player_ctx->m_buffer->DVD()->IsInMenu() &&
            !player_ctx->m_buffer->DVD()->IsInStillFrame())
        {
            GoToDVDProgram(true);
        }
    }
    else if (player_ctx->m_buffer->DVD()->GetTotalTimeOfTitle() < 60)
    {
        GoToDVDProgram(true);
        Number = FrameNum;
        if (Number >= totalFrames)
            Number = totalFrames / 2;
    }
}

int MythDVDPlayer::SetTrack(uint Type, int TrackNo)
{
    if (kTrackTypeAudio == Type)
    {
        StreamInfo stream = decoder->GetTrackInfo(Type, static_cast<uint>(TrackNo));
        player_ctx->m_buffer->DVD()->SetTrack(Type, stream.m_stream_id);
    }

    return MythPlayer::SetTrack(Type, TrackNo);
}

int MythDVDPlayer::GetNumChapters(void)
{
    if (!player_ctx->m_buffer->IsDVD())
        return 0;
    return player_ctx->m_buffer->DVD()->NumPartsInTitle();
}

int MythDVDPlayer::GetCurrentChapter(void)
{
    if (!player_ctx->m_buffer->IsDVD())
        return 0;
    return player_ctx->m_buffer->DVD()->GetPart();
}

void MythDVDPlayer::GetChapterTimes(QList<long long> &Times)
{
    if (!player_ctx->m_buffer->IsDVD())
        return;
    player_ctx->m_buffer->DVD()->GetChapterTimes(Times);
}

bool MythDVDPlayer::DoJumpChapter(int Chapter)
{
    if (!player_ctx->m_buffer->IsDVD())
        return false;

    int total   = GetNumChapters();
    int current = GetCurrentChapter();

    if (Chapter < 0 || Chapter > total)
    {
        if (Chapter < 0)
        {
            Chapter = current -1;
            if (Chapter < 0) Chapter = 0;
        }
        else if (Chapter > total)
        {
            Chapter = current + 1;
            if (Chapter > total) Chapter = total;
        }
    }

    bool success = player_ctx->m_buffer->DVD()->playTrack(Chapter);
    if (success)
    {
        if (decoder)
        {
            decoder->UpdateFramesPlayed();
            if (player_ctx->m_buffer->DVD()->GetCellStart() == 0)
                decoder->SeekReset(static_cast<long long>(framesPlayed), 0, true, true);
        }
        ClearAfterSeek(!player_ctx->m_buffer->IsInDiscMenuOrStillFrame());
    }

    jumpchapter = 0;
    return success;
}

void MythDVDPlayer::DisplayDVDButton(void)
{
    if (!osd || !player_ctx->m_buffer->IsDVD())
        return;

    uint buttonversion = 0;
    AVSubtitle *dvdSubtitle = player_ctx->m_buffer->DVD()->GetMenuSubtitle(buttonversion);
    bool numbuttons    = player_ctx->m_buffer->DVD()->NumMenuButtons() != 0;

    bool expired = false;

    VideoFrame *currentFrame = videoOutput ? videoOutput->GetLastShownFrame() : nullptr;

    if (!currentFrame)
    {
        player_ctx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if (dvdSubtitle &&
        (dvdSubtitle->end_display_time > dvdSubtitle->start_display_time) &&
        (dvdSubtitle->end_display_time < currentFrame->timecode))
    {
        expired = true;
    }

    // nothing to do
    if (!expired && (buttonversion == (static_cast<uint>(m_buttonVersion))))
    {
        player_ctx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    // clear any buttons
    if (!numbuttons || !dvdSubtitle || (buttonversion == 0) || expired)
    {
        osdLock.lock();
        if (osd)
            osd->ClearSubtitles();
        osdLock.unlock();
        m_buttonVersion = 0;
        player_ctx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if (currentFrame->timecode && (dvdSubtitle->start_display_time > currentFrame->timecode))
    {
        player_ctx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    m_buttonVersion = static_cast<int>(buttonversion);
    QRect buttonPos = player_ctx->m_buffer->DVD()->GetButtonCoords();
    osdLock.lock();
    if (osd)
        osd->DisplayDVDButton(dvdSubtitle, buttonPos);
    osdLock.unlock();
    textDisplayMode = kDisplayDVDButton;
    player_ctx->m_buffer->DVD()->ReleaseMenuButton();
}

bool MythDVDPlayer::GoToMenu(QString Menu)
{
    if (!player_ctx->m_buffer->IsDVD())
        return false;
    textDisplayMode = kDisplayNone;
    bool ret = player_ctx->m_buffer->DVD()->GoToMenu(Menu);

    if (!ret)
    {
        SetOSDMessage(tr("DVD Menu Not Available"), kOSDTimeout_Med);
        LOG(VB_GENERAL, LOG_ERR, "No DVD Menu available.");
        return false;
    }

    return true;
}

void MythDVDPlayer::GoToDVDProgram(bool Direction)
{
    if (!player_ctx->m_buffer->IsDVD())
        return;
    if (Direction)
        player_ctx->m_buffer->DVD()->GoToPreviousProgram();
    else
        player_ctx->m_buffer->DVD()->GoToNextProgram();
}

bool MythDVDPlayer::IsInStillFrame() const
{
    return (m_stillFrameLength > 0);
}

int MythDVDPlayer::GetNumAngles(void) const
{
    if (player_ctx->m_buffer->DVD() && player_ctx->m_buffer->DVD()->IsOpen())
        return player_ctx->m_buffer->DVD()->GetNumAngles();
    return 0;
}

int MythDVDPlayer::GetCurrentAngle(void) const
{
    if (player_ctx->m_buffer->DVD() && player_ctx->m_buffer->DVD()->IsOpen())
        return player_ctx->m_buffer->DVD()->GetCurrentAngle();
    return -1;
}

QString MythDVDPlayer::GetAngleName(int Angle) const
{
    if (Angle >= 1 && Angle <= GetNumAngles())
    {
        QString name = tr("Angle %1").arg(Angle);
        return name;
    }
    return QString();
}

bool MythDVDPlayer::SwitchAngle(int Angle)
{
    int total = GetNumAngles();
    if (!total || Angle == GetCurrentAngle())
        return false;

    if (Angle < 1 || Angle > total)
        Angle = 1;

    return player_ctx->m_buffer->DVD()->SwitchAngle(static_cast<uint>(Angle));
}

void MythDVDPlayer::ResetStillFrameTimer(void)
{
    m_stillFrameTimerLock.lock();
    m_stillFrameTimer.restart();
    m_stillFrameTimerLock.unlock();
}

void MythDVDPlayer::SetStillFrameTimeout(int Length)
{
    if (Length != m_stillFrameLength)
    {
        m_stillFrameTimerLock.lock();
        m_stillFrameLength = Length;
        m_stillFrameTimer.restart();
        m_stillFrameTimerLock.unlock();
    }
}

void MythDVDPlayer::StillFrameCheck(void)
{
    if (player_ctx->m_buffer->IsDVD() &&
        player_ctx->m_buffer->DVD()->IsInStillFrame() &&
       (m_stillFrameLength > 0) && (m_stillFrameLength < 0xff))
    {
        m_stillFrameTimerLock.lock();
        int elapsedTime = static_cast<int>(m_stillFrameTimer.elapsed() * play_speed / 1000.0F);
        m_stillFrameTimerLock.unlock();
        if (elapsedTime >= m_stillFrameLength)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Stillframe timeout after %1 seconds (timestretch %2)")
                    .arg(m_stillFrameLength).arg(static_cast<double>(play_speed)));
            player_ctx->m_buffer->DVD()->SkipStillFrame();
            m_stillFrameLength = 0;
        }
    }
}

void MythDVDPlayer::CreateDecoder(char *Testbuf, int TestReadSize)
{
    if (AvFormatDecoderDVD::CanHandle(Testbuf, player_ctx->m_buffer->GetFilename(),
                                      TestReadSize))
    {
        SetDecoder(new AvFormatDecoderDVD(this, *player_ctx->m_playingInfo,
                                          playerFlags));
    }
}
