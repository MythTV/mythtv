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
    if (m_decoder && m_decoder->GetMythCodecContext()->IsDeinterlacing(dummy))
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
                        !m_playerCtx->m_buffer->IsInDiscMenuOrStillFrame());
}

bool MythDVDPlayer::HasReachedEof(void) const
{
    EofState eof = GetEof();
    // DeleteMap and EditMode from the parent MythPlayer should not be
    // relevant here.
    return eof != kEofStateNone && !m_allPaused;
}

void MythDVDPlayer::DisableCaptions(uint Mode, bool OSDMsg)
{
    if ((kDisplayAVSubtitle & Mode) && m_playerCtx->m_buffer->IsDVD())
        m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle, -1);
    MythPlayer::DisableCaptions(Mode, OSDMsg);
}

void MythDVDPlayer::EnableCaptions(uint Mode, bool OSDMsg)
{
    if ((kDisplayAVSubtitle & Mode) && m_playerCtx->m_buffer->IsDVD())
    {
        int track = GetTrack(kTrackTypeSubtitle);
        if (track >= 0 && track < static_cast<int>(m_decoder->GetTrackCount(kTrackTypeSubtitle)))
        {
            StreamInfo stream = m_decoder->GetTrackInfo(kTrackTypeSubtitle,
                                                      static_cast<uint>(track));
            m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle,
                                                stream.m_stream_id);
        }
    }
    MythPlayer::EnableCaptions(Mode, OSDMsg);
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

bool MythDVDPlayer::PrebufferEnoughFrames(int /*MinBuffers*/)
{
    return MythPlayer::PrebufferEnoughFrames(1);
}

bool MythDVDPlayer::DecoderGetFrameFFREW(void)
{
    bool res = MythPlayer::DecoderGetFrameFFREW();
    if (m_decoderChangeLock.tryLock(1))
    {
        if (m_decoder)
            m_decoder->UpdateFramesPlayed();
        m_decoderChangeLock.unlock();
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
    if (!m_initialDvdState.isEmpty())
        m_playerCtx->m_buffer->DVD()->RestoreDVDStateSnapshot(m_initialDvdState);

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
    if (m_videoOutput)
        nbframes = m_videoOutput->ValidVideoFrames();

#if 0
    LOG(VB_PLAYBACK, LOG_DEBUG,
        LOC + QString("Validframes %1, FreeFrames %2, VideoPaused %3")
           .arg(nbframes).arg(videoOutput->FreeVideoFrames()).arg(videoPaused));
#endif

    // completely drain the video buffers for certain situations
    bool release_all = m_playerCtx->m_buffer->DVD()->DVDWaitingForPlayer() &&
                      (nbframes > 0);
    bool release_one = (nbframes > 1) && m_videoPaused && !m_allPaused &&
                       (!m_videoOutput->EnoughFreeFrames() ||
                        m_playerCtx->m_buffer->DVD()->IsWaiting() ||
                        m_playerCtx->m_buffer->DVD()->IsInStillFrame());
    if (release_all || release_one)
    {
        if (nbframes < 5 && m_videoOutput)
            m_videoOutput->UpdatePauseFrame(m_dispTimecode);

        // if we go below the pre-buffering limit, the player will pause
        // so do this 'manually'
        DisplayNormalFrame(false);
        // unpause the still frame if more frames become available
        if (m_dvdStillFrameShowing && nbframes > 1)
        {
            m_dvdStillFrameShowing = false;
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
        if (m_videoPaused && !m_allPaused)
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
            if (m_videoPaused && !m_allPaused)
                UnpauseVideo();
            return !IsErrored();
        }

        // the still frame is treated as a pause frame
        if (m_playerCtx->m_buffer->DVD()->IsStillFramePending())
        {
            // ensure we refresh the pause frame
            if (!m_dvdStillFrameShowing)
                m_needNewPauseFrame = true;

            // we are in a still frame so pause video output
            if (!m_videoPaused)
            {
                PauseVideo();
                m_dvdStillFrameShowing = true;
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

            m_dvdStillFrameShowing = true;
        }
    }

    // unpause the still frame if more frames become available
    if (m_dvdStillFrameShowing && nbframes > 1)
    {
        UnpauseVideo();
        m_dvdStillFrameShowing = false;
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
    if (m_decoder)
        m_decoder->UpdateFramesPlayed();
    return MythPlayer::FastForward(Seconds);
}

bool MythDVDPlayer::Rewind(float Seconds)
{
    if (m_decoder)
        m_decoder->UpdateFramesPlayed();
    return MythPlayer::Rewind(Seconds);
}

bool MythDVDPlayer::JumpToFrame(uint64_t Frame)
{
    if (Frame == ~0x0ULL)
        return false;

    if (m_decoder)
        m_decoder->UpdateFramesPlayed();
    return MythPlayer::JumpToFrame(Frame);
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

    if (m_initialTitle > -1)
        m_playerCtx->m_buffer->DVD()->PlayTitleAndPart(m_initialTitle, 1);

    if (m_initialAudioTrack > -1)
        m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeAudio,
                                               m_initialAudioTrack);
    if (m_initialSubtitleTrack > -1)
        m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle,
                                               m_initialSubtitleTrack);

    if (m_bookmarkSeek > 30)
    {
        // we need to trigger a dvd cell change to ensure the new title length
        // is set and the position map updated accordingly
        m_decodeOneFrame = true;
        int count = 0;
        while (count++ < 100 && m_decodeOneFrame)
            usleep(50000);
    }
    MythPlayer::InitialSeek();
    m_playerCtx->m_buffer->IgnoreWaitStates(false);
}

void MythDVDPlayer::ResetPlaying(bool /*ResetFrames*/)
{
    MythPlayer::ResetPlaying(false);
}

void MythDVDPlayer::EventEnd(void)
{
    if (m_playerCtx->m_buffer->DVD())
        m_playerCtx->m_buffer->DVD()->SetParent(nullptr);
}

bool MythDVDPlayer::PrepareAudioSample(int64_t &Timecode)
{
    if (!m_playerCtx->m_buffer->IsInDiscMenuOrStillFrame())
        WrapTimecode(Timecode, TC_AUDIO);

    return m_playerCtx->m_buffer->IsDVD() &&
           m_playerCtx->m_buffer->DVD()->IsInStillFrame();
}

void MythDVDPlayer::SetBookmark(bool Clear)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return;

    QStringList fields;
    QString name;
    QString serialid;
    QString dvdstate;

    if (!m_playerCtx->m_buffer->IsInMenu() &&
        (m_playerCtx->m_buffer->IsBookmarkAllowed() || Clear))
    {
        if (!m_playerCtx->m_buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "DVD has no name and serial number. Cannot set bookmark.");
            return;
        }

        if (!Clear && !m_playerCtx->m_buffer->DVD()->GetDVDStateSnapshot(dvdstate))
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

            if (!Clear)
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
                m_initialDvdState = *it;
                frames = ~0x0ULL;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Get Bookmark: bookmark found");
            }
            else
            {
                // Legacy bookmarks
                m_initialTitle = (*it).toInt();
                frames = ((*++it).toLongLong() & 0xffffffffLL);
                m_initialAudioTrack    = (*++it).toInt();
                m_initialSubtitleTrack = (*++it).toInt();
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Get Bookmark: title %1 audiotrack %2 subtrack %3 "
                            "frame %4")
                    .arg(m_initialTitle).arg(m_initialAudioTrack)
                    .arg(m_initialSubtitleTrack).arg(frames));
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
        int elapsed = static_cast<int>((m_stillFrameTimer.elapsed() * m_playSpeed / m_nextPlaySpeed));
        m_stillFrameTimer.restart();
        m_stillFrameTimer.addMSecs(elapsed);
        m_stillFrameTimerLock.unlock();
    }

    MythPlayer::ChangeSpeed();

    if (m_decoder)
        m_decoder->UpdateFramesPlayed();
    if (m_playerCtx->m_buffer->IsDVD())
    {
        if (m_playSpeed > 1.0F)
            m_playerCtx->m_buffer->DVD()->SetDVDSpeed(-1);
        else
            m_playerCtx->m_buffer->DVD()->SetDVDSpeed();
    }
}

long long MythDVDPlayer::CalcMaxFFTime(long long FastFwd, bool Setjump) const
{
    if ((m_totalFrames > 0) && m_playerCtx->m_buffer->IsDVD() &&
        m_playerCtx->m_buffer->DVD()->TitleTimeLeft() < 5)
        return 0;
    return MythPlayer::CalcMaxFFTime(FastFwd, Setjump);
}

int64_t MythDVDPlayer::GetSecondsPlayed(bool /*HonorCutList*/, int Divisor)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return 0;

    int64_t played = m_playerCtx->m_buffer->DVD()->GetCurrentTime();

    if (m_stillFrameLength > 0)
    {
        if (m_stillFrameLength == 255)
            played = -1;
        else
            played = static_cast<int64_t>(m_stillFrameTimer.elapsed() * m_playSpeed / Divisor);
    }

    return played;
}

int64_t MythDVDPlayer::GetTotalSeconds(bool /*HonorCutList*/, int Divisor) const
{
    int64_t total = m_totalLength;

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
        Number = FrameNum;
        if (Number >= m_totalFrames)
            Number = m_totalFrames / 2;
    }
}

int MythDVDPlayer::SetTrack(uint Type, int TrackNo)
{
    if (kTrackTypeAudio == Type)
    {
        StreamInfo stream = m_decoder->GetTrackInfo(Type, static_cast<uint>(TrackNo));
        m_playerCtx->m_buffer->DVD()->SetTrack(Type, stream.m_stream_id);
    }

    return MythPlayer::SetTrack(Type, TrackNo);
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

void MythDVDPlayer::GetChapterTimes(QList<long long> &Times)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return;
    m_playerCtx->m_buffer->DVD()->GetChapterTimes(Times);
}

bool MythDVDPlayer::DoJumpChapter(int Chapter)
{
    if (!m_playerCtx->m_buffer->IsDVD())
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

    bool success = m_playerCtx->m_buffer->DVD()->playTrack(Chapter);
    if (success)
    {
        if (m_decoder)
        {
            m_decoder->UpdateFramesPlayed();
            if (m_playerCtx->m_buffer->DVD()->GetCellStart() == 0)
                m_decoder->SeekReset(static_cast<long long>(m_framesPlayed), 0, true, true);
        }
        ClearAfterSeek(!m_playerCtx->m_buffer->IsInDiscMenuOrStillFrame());
    }

    m_jumpChapter = 0;
    return success;
}

void MythDVDPlayer::DisplayDVDButton(void)
{
    if (!m_osd || !m_playerCtx->m_buffer->IsDVD())
        return;

    uint buttonversion = 0;
    AVSubtitle *dvdSubtitle = m_playerCtx->m_buffer->DVD()->GetMenuSubtitle(buttonversion);
    bool numbuttons    = m_playerCtx->m_buffer->DVD()->NumMenuButtons() != 0;

    bool expired = false;

    VideoFrame *currentFrame = m_videoOutput ? m_videoOutput->GetLastShownFrame() : nullptr;

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
    if (!expired && (buttonversion == (static_cast<uint>(m_buttonVersion))))
    {
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    // clear any buttons
    if (!numbuttons || !dvdSubtitle || (buttonversion == 0) || expired)
    {
        m_osdLock.lock();
        if (m_osd)
            m_osd->ClearSubtitles();
        m_osdLock.unlock();
        m_buttonVersion = 0;
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if (currentFrame->timecode && (dvdSubtitle->start_display_time > currentFrame->timecode))
    {
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    m_buttonVersion = static_cast<int>(buttonversion);
    QRect buttonPos = m_playerCtx->m_buffer->DVD()->GetButtonCoords();
    m_osdLock.lock();
    if (m_osd)
        m_osd->DisplayDVDButton(dvdSubtitle, buttonPos);
    m_osdLock.unlock();
    m_textDisplayMode = kDisplayDVDButton;
    m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
}

bool MythDVDPlayer::GoToMenu(QString Menu)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return false;
    m_textDisplayMode = kDisplayNone;
    bool ret = m_playerCtx->m_buffer->DVD()->GoToMenu(Menu);

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
    if (!m_playerCtx->m_buffer->IsDVD())
        return;
    if (Direction)
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

    return m_playerCtx->m_buffer->DVD()->SwitchAngle(static_cast<uint>(Angle));
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
    if (m_playerCtx->m_buffer->IsDVD() &&
        m_playerCtx->m_buffer->DVD()->IsInStillFrame() &&
       (m_stillFrameLength > 0) && (m_stillFrameLength < 0xff))
    {
        m_stillFrameTimerLock.lock();
        int elapsedTime = static_cast<int>(m_stillFrameTimer.elapsed() * m_playSpeed / 1000.0F);
        m_stillFrameTimerLock.unlock();
        if (elapsedTime >= m_stillFrameLength)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Stillframe timeout after %1 seconds (timestretch %2)")
                .arg(m_stillFrameLength).arg(static_cast<double>(m_playSpeed)));
            m_playerCtx->m_buffer->DVD()->SkipStillFrame();
            m_stillFrameLength = 0;
        }
    }
}

void MythDVDPlayer::CreateDecoder(char *Testbuf, int TestReadSize)
{
    if (AvFormatDecoderDVD::CanHandle(Testbuf, m_playerCtx->m_buffer->GetFilename(),
                                      TestReadSize))
    {
        SetDecoder(new AvFormatDecoderDVD(this, *m_playerCtx->m_playingInfo,
                                          m_playerFlags));
    }
}
