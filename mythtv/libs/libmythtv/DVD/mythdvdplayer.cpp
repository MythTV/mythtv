#include "dvdringbuffer.h"
#include "DetectLetterbox.h"
#include "audiooutput.h"
#include "avformatdecoderdvd.h"
#include "mythdvdplayer.h"

#include <unistd.h> // for usleep()

#define LOC      QString("DVDPlayer: ")

void MythDVDPlayer::AutoDeint(VideoFrame *frame, bool allow_lock)
{
    bool dummy = false;
    if (decoder && decoder->GetMythCodecContext()->IsDeinterlacing(dummy))
    {
        MythPlayer::AutoDeint(frame, allow_lock);
        return;
    }

    (void)frame;
    (void)allow_lock;
    SetScanType(kScan_Interlaced);
}

/** \fn MythDVDPlayer::ReleaseNextVideoFrame(VideoFrame*, int64_t)
 *  \param buffer    Buffer
 *  \param timecode  Timecode
 *  \param wrap      Ignored. This function overrides the callers
 *                   'wrap' indication and computes its own based on
 *                   whether or not video is currently playing.
 */
void MythDVDPlayer::ReleaseNextVideoFrame(VideoFrame *buffer,
                                          int64_t timecode, bool /*wrap*/)
{
    MythPlayer::ReleaseNextVideoFrame(buffer, timecode,
                        !m_playerCtx->m_buffer->IsInDiscMenuOrStillFrame());
}

bool MythDVDPlayer::HasReachedEof(void) const
{
    EofState eof = GetEof();
    // DeleteMap and EditMode from the parent MythPlayer should not be
    // relevant here.
    return eof != kEofStateNone && !allpaused;
}

void MythDVDPlayer::DisableCaptions(uint mode, bool osd_msg)
{
    if ((kDisplayAVSubtitle & mode) && m_playerCtx->m_buffer->IsDVD())
        m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle, -1);
    MythPlayer::DisableCaptions(mode, osd_msg);
}

void MythDVDPlayer::EnableCaptions(uint mode, bool osd_msg)
{
    if ((kDisplayAVSubtitle & mode) && m_playerCtx->m_buffer->IsDVD())
    {
        int track = GetTrack(kTrackTypeSubtitle);
        if (track >= 0 && track < (int)decoder->GetTrackCount(kTrackTypeSubtitle))
        {
            StreamInfo stream = decoder->GetTrackInfo(kTrackTypeSubtitle,
                                                      track);
            m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle,
                                                   stream.m_stream_id);
        }
    }
    MythPlayer::EnableCaptions(mode, osd_msg);
}

void MythDVDPlayer::DisplayPauseFrame(void)
{
    if (m_playerCtx->m_buffer->IsDVD() &&
        m_playerCtx->m_buffer->DVD()->IsInStillFrame())
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

bool MythDVDPlayer::PrebufferEnoughFrames(int /*min_buffers*/)
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
    return (m_playerCtx->m_buffer->IsDVD() &&
            (m_playerCtx->m_buffer->DVD()->GetCurrentTime() < 2));
}

void MythDVDPlayer::PreProcessNormalFrame(void)
{
    DisplayDVDButton();
}

void MythDVDPlayer::VideoStart(void)
{
    if (!m_initial_dvdstate.isEmpty())
        m_playerCtx->m_buffer->DVD()->RestoreDVDStateSnapshot(m_initial_dvdstate);

    MythPlayer::VideoStart();
}

bool MythDVDPlayer::VideoLoop(void)
{
    if (!m_playerCtx->m_buffer->IsDVD())
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
    bool release_all = m_playerCtx->m_buffer->DVD()->DVDWaitingForPlayer() &&
                      (nbframes > 0);
    bool release_one = (nbframes > 1) && videoPaused && !allpaused &&
                       (!videoOutput->EnoughFreeFrames() ||
                        m_playerCtx->m_buffer->DVD()->IsWaiting() ||
                        m_playerCtx->m_buffer->DVD()->IsInStillFrame());
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
    if (m_playerCtx->m_buffer->DVD()->DVDWaitingForPlayer())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing MythTV DVD wait state");
        m_playerCtx->m_buffer->DVD()->SkipDVDWaitingForPlayer();
        ClearAfterSeek(true);
        if (videoPaused && !allpaused)
            UnpauseVideo();
        return !IsErrored();
    }

    // wait for the video buffers to drain
    if (nbframes < 2)
    {
        // clear the DVD wait state
        if (m_playerCtx->m_buffer->DVD()->IsWaiting())
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing DVD wait state");
            m_playerCtx->m_buffer->DVD()->WaitSkip();
            if (videoPaused && !allpaused)
                UnpauseVideo();
            return !IsErrored();
        }

        // the still frame is treated as a pause frame
        if (m_playerCtx->m_buffer->DVD()->IsStillFramePending())
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

    AVSync(nullptr, true);
}

bool MythDVDPlayer::FastForward(float seconds)
{
    if (decoder)
        decoder->UpdateFramesPlayed();
    return MythPlayer::FastForward(seconds);
}

bool MythDVDPlayer::Rewind(float seconds)
{
    if (decoder)
        decoder->UpdateFramesPlayed();
    return MythPlayer::Rewind(seconds);
}

bool MythDVDPlayer::JumpToFrame(uint64_t frame)
{
    if (frame == ~0x0ULL)
        return false;

    if (decoder)
        decoder->UpdateFramesPlayed();
    return MythPlayer::JumpToFrame(frame);
}

void MythDVDPlayer::EventStart(void)
{
    if (m_playerCtx->m_buffer->DVD())
        m_playerCtx->m_buffer->DVD()->SetParent(this);

    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
    {
        QString name;
        QString serialid;
        if (m_playerCtx->m_playingInfo->GetTitle().isEmpty() &&
            m_playerCtx->m_buffer->DVD() &&
            m_playerCtx->m_buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            m_playerCtx->m_playingInfo->SetTitle(name);
        }
    }
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    MythPlayer::EventStart();
}

void MythDVDPlayer::InitialSeek(void)
{
    m_playerCtx->m_buffer->IgnoreWaitStates(true);

    if (m_initial_title > -1)
        m_playerCtx->m_buffer->DVD()->PlayTitleAndPart(m_initial_title, 1);

    if (m_initial_audio_track > -1)
        m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeAudio,
                                               m_initial_audio_track);
    if (m_initial_subtitle_track > -1)
        m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle,
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
    m_playerCtx->m_buffer->IgnoreWaitStates(false);
}

void MythDVDPlayer::ResetPlaying(bool /*resetframes*/)
{
    MythPlayer::ResetPlaying(false);
}

void MythDVDPlayer::EventEnd(void)
{
    if (m_playerCtx->m_buffer->DVD())
        m_playerCtx->m_buffer->DVD()->SetParent(nullptr);
}

bool MythDVDPlayer::PrepareAudioSample(int64_t &timecode)
{
    if (!m_playerCtx->m_buffer->IsInDiscMenuOrStillFrame())
        WrapTimecode(timecode, TC_AUDIO);

    return m_playerCtx->m_buffer->IsDVD() &&
           m_playerCtx->m_buffer->DVD()->IsInStillFrame();
}

void MythDVDPlayer::SetBookmark(bool clear)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return;

    QStringList fields;
    QString name;
    QString serialid;
    QString dvdstate;

    if (!m_playerCtx->m_buffer->IsInMenu() &&
        (m_playerCtx->m_buffer->IsBookmarkAllowed() || clear))
    {
        if (!m_playerCtx->m_buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "DVD has no name and serial number. Cannot set bookmark.");
            return;
        }

        if (!clear && !m_playerCtx->m_buffer->DVD()->GetDVDStateSnapshot(dvdstate))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unable to retrieve DVD state. Cannot set bookmark.");
            return;
        }

        m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
        if (m_playerCtx->m_playingInfo)
        {
            fields += serialid;
            fields += name;

            if (!clear)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Set bookmark");
                fields += dvdstate;
            }
            else
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clear bookmark");

            ProgramInfo::SaveDVDBookmark(fields);

        }
        m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    }
}

uint64_t MythDVDPlayer::GetBookmark(void)
{
    if (gCoreContext->IsDatabaseIgnored() || !m_playerCtx->m_buffer->IsDVD())
        return 0;

    QString name;
    QString serialid;
    uint64_t frames = 0;
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
    {
        if (!m_playerCtx->m_buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
            return 0;
        }

        QStringList dvdbookmark = m_playerCtx->m_playingInfo->QueryDVDBookmark(serialid);

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
                frames = (uint64_t)((*++it).toLongLong() & 0xffffffffLL);
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
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
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
        int elapsed = (int)(m_stillFrameTimer.elapsed() * play_speed / next_play_speed);
        m_stillFrameTimer.restart();
        m_stillFrameTimer.addMSecs(elapsed);
        m_stillFrameTimerLock.unlock();
    }

    MythPlayer::ChangeSpeed();

    if (decoder)
        decoder->UpdateFramesPlayed();
    if (m_playerCtx->m_buffer->IsDVD())
    {
        if (play_speed > 1.0F)
            m_playerCtx->m_buffer->DVD()->SetDVDSpeed(-1);
        else
            m_playerCtx->m_buffer->DVD()->SetDVDSpeed();
    }
}

void MythDVDPlayer::AVSync(VideoFrame *frame, bool /*limit_delay*/)
{
    MythPlayer::AVSync(frame, true);
}

long long MythDVDPlayer::CalcMaxFFTime(long long ff, bool setjump) const
{
    if ((totalFrames > 0) && m_playerCtx->m_buffer->IsDVD() &&
        m_playerCtx->m_buffer->DVD()->TitleTimeLeft() < 5)
        return 0;
    return MythPlayer::CalcMaxFFTime(ff, setjump);
}

int64_t MythDVDPlayer::GetSecondsPlayed(bool /*honorCutList*/, int divisor)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return 0;

    int64_t played = m_playerCtx->m_buffer->DVD()->GetCurrentTime();

    if (m_stillFrameLength > 0)
    {
        if (m_stillFrameLength == 255)
            played = -1;
        else
            played = m_stillFrameTimer.elapsed() * play_speed / divisor;
    }

    return played;
}

int64_t MythDVDPlayer::GetTotalSeconds(bool /*honorCutList*/, int divisor) const
{
    int64_t total = totalLength;

    if (m_stillFrameLength > 0)
    {
        if (m_stillFrameLength == 255)
            return -1;
        total = m_stillFrameLength;
    }

    return total * 1000 / divisor;
}

void MythDVDPlayer::SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                      bool /*absolute*/)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return;
    if (GoToMenu("menu"))
    {
        if (m_playerCtx->m_buffer->DVD()->IsInMenu() &&
            !m_playerCtx->m_buffer->DVD()->IsInStillFrame())
        {
            GoToDVDProgram(true);
        }
    }
    else if (m_playerCtx->m_buffer->DVD()->GetTotalTimeOfTitle() < 60)
    {
        GoToDVDProgram(true);
        number = frameNum;
        if (number >= totalFrames)
            number = totalFrames / 2;
    }
}

int MythDVDPlayer::SetTrack(uint type, int trackNo)
{
    if (kTrackTypeAudio == type)
    {
        StreamInfo stream = decoder->GetTrackInfo(type, trackNo);
        m_playerCtx->m_buffer->DVD()->SetTrack(type, stream.m_stream_id);
    }

    return MythPlayer::SetTrack(type, trackNo);
}

int MythDVDPlayer::GetNumChapters(void)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return 0;
    return m_playerCtx->m_buffer->DVD()->NumPartsInTitle();
}

int MythDVDPlayer::GetCurrentChapter(void)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return 0;
    return m_playerCtx->m_buffer->DVD()->GetPart();
}

void MythDVDPlayer::GetChapterTimes(QList<long long> &times)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return;
    m_playerCtx->m_buffer->DVD()->GetChapterTimes(times);
}

bool MythDVDPlayer::DoJumpChapter(int chapter)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return false;

    int total   = GetNumChapters();
    int current = GetCurrentChapter();

    if (chapter < 0 || chapter > total)
    {
        if (chapter < 0)
        {
            chapter = current -1;
            if (chapter < 0) chapter = 0;
        }
        else if (chapter > total)
        {
            chapter = current + 1;
            if (chapter > total) chapter = total;
        }
    }

    bool success = m_playerCtx->m_buffer->DVD()->playTrack(chapter);
    if (success)
    {
        if (decoder)
        {
            decoder->UpdateFramesPlayed();
            if (m_playerCtx->m_buffer->DVD()->GetCellStart() == 0)
                decoder->SeekReset(framesPlayed, 0, true, true);
        }
        ClearAfterSeek(!m_playerCtx->m_buffer->IsInDiscMenuOrStillFrame());
    }

    jumpchapter = 0;
    return success;
}

void MythDVDPlayer::DisplayDVDButton(void)
{
    if (!osd || !m_playerCtx->m_buffer->IsDVD())
        return;

    uint buttonversion = 0;
    AVSubtitle *dvdSubtitle = m_playerCtx->m_buffer->DVD()->GetMenuSubtitle(buttonversion);
    bool numbuttons    = m_playerCtx->m_buffer->DVD()->NumMenuButtons() != 0;

    bool expired = false;

    VideoFrame *currentFrame = videoOutput ? videoOutput->GetLastShownFrame() : nullptr;

    if (!currentFrame)
    {
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if (dvdSubtitle &&
        (dvdSubtitle->end_display_time > dvdSubtitle->start_display_time) &&
        (dvdSubtitle->end_display_time < currentFrame->timecode))
    {
        expired = true;
    }

    // nothing to do
    if (!expired && (buttonversion == ((uint)m_buttonVersion)))
    {
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
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
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if (currentFrame->timecode && (dvdSubtitle->start_display_time > currentFrame->timecode))
    {
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    m_buttonVersion = buttonversion;
    QRect buttonPos = m_playerCtx->m_buffer->DVD()->GetButtonCoords();
    osdLock.lock();
    if (osd)
        osd->DisplayDVDButton(dvdSubtitle, buttonPos);
    osdLock.unlock();
    textDisplayMode = kDisplayDVDButton;
    m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
}

bool MythDVDPlayer::GoToMenu(QString str)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return false;
    textDisplayMode = kDisplayNone;
    bool ret = m_playerCtx->m_buffer->DVD()->GoToMenu(str);

    if (!ret)
    {
        SetOSDMessage(tr("DVD Menu Not Available"), kOSDTimeout_Med);
        LOG(VB_GENERAL, LOG_ERR, "No DVD Menu available.");
        return false;
    }

    return true;
}

void MythDVDPlayer::GoToDVDProgram(bool direction)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return;
    if (direction)
        m_playerCtx->m_buffer->DVD()->GoToPreviousProgram();
    else
        m_playerCtx->m_buffer->DVD()->GoToNextProgram();
}

bool MythDVDPlayer::IsInStillFrame() const
{
    return (m_stillFrameLength > 0);
}

int MythDVDPlayer::GetNumAngles(void) const
{
    if (m_playerCtx->m_buffer->DVD() && m_playerCtx->m_buffer->DVD()->IsOpen())
        return m_playerCtx->m_buffer->DVD()->GetNumAngles();
    return 0;
}

int MythDVDPlayer::GetCurrentAngle(void) const
{
    if (m_playerCtx->m_buffer->DVD() && m_playerCtx->m_buffer->DVD()->IsOpen())
        return m_playerCtx->m_buffer->DVD()->GetCurrentAngle();
    return -1;
}

QString MythDVDPlayer::GetAngleName(int angle) const
{
    if (angle >= 1 && angle <= GetNumAngles())
    {
        QString name = tr("Angle %1").arg(angle);
        return name;
    }
    return QString();
}

bool MythDVDPlayer::SwitchAngle(int angle)
{
    uint total = GetNumAngles();
    if (!total || angle == GetCurrentAngle())
        return false;

    if (angle < 1 || angle > (int)total)
        angle = 1;

    return m_playerCtx->m_buffer->DVD()->SwitchAngle(angle);
}

void MythDVDPlayer::ResetStillFrameTimer(void)
{
    m_stillFrameTimerLock.lock();
    m_stillFrameTimer.restart();
    m_stillFrameTimerLock.unlock();
}

void MythDVDPlayer::SetStillFrameTimeout(int length)
{
    if (length != m_stillFrameLength)
    {
        m_stillFrameTimerLock.lock();
        m_stillFrameLength = length;
        m_stillFrameTimer.restart();
        m_stillFrameTimerLock.unlock();
    }
}

void MythDVDPlayer::StillFrameCheck(void)
{
    if (m_playerCtx->m_buffer->IsDVD() &&
        m_playerCtx->m_buffer->DVD()->IsInStillFrame() &&
       (m_stillFrameLength > 0) && (m_stillFrameLength < 0xff))
    {
        m_stillFrameTimerLock.lock();
        int elapsedTime = (int)(m_stillFrameTimer.elapsed() * play_speed / 1000);
        m_stillFrameTimerLock.unlock();
        if (elapsedTime >= m_stillFrameLength)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Stillframe timeout after %1 seconds (timestretch %2)")
                    .arg(m_stillFrameLength)
                    .arg(play_speed));
            m_playerCtx->m_buffer->DVD()->SkipStillFrame();
            m_stillFrameLength = 0;
        }
    }
}

void MythDVDPlayer::CreateDecoder(char *testbuf, int testreadsize)
{
    if (AvFormatDecoderDVD::CanHandle(testbuf, m_playerCtx->m_buffer->GetFilename(),
                                      testreadsize))
    {
        SetDecoder(new AvFormatDecoderDVD(this, *m_playerCtx->m_playingInfo,
                                          playerFlags));
    }
}
