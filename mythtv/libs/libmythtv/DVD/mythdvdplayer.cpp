#include <algorithm>

// MythTV
#include "libmyth/audio/audiooutput.h"

#include "DVD/mythdvdbuffer.h"
#include "DVD/mythdvddecoder.h"
#include "DVD/mythdvdplayer.h"
#include "tv_play.h"

#define LOC      QString("DVDPlayer: ")

MythDVDPlayer::MythDVDPlayer(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayerUI(MainWindow, Tv, Context, Flags)
{
    connect(Tv, &TV::GoToMenu, this, &MythDVDPlayer::GoToMenu);
    connect(Tv, &TV::GoToDVDProgram, this, &MythDVDPlayer::GoToDVDProgram);
    connect(this, &MythDVDPlayer::DisableDVDSubtitles, this, &MythDVDPlayer::DoDisableDVDSubtitles);
}

void MythDVDPlayer::AutoDeint(MythVideoFrame *Frame, MythVideoOutput *VideoOutput,
                              std::chrono::microseconds FrameInterval, bool AllowLock)
{
    bool dummy = false;
    if (m_decoder && m_decoder->GetMythCodecContext()->IsDeinterlacing(dummy))
    {
        MythPlayerUI::AutoDeint(Frame, VideoOutput, FrameInterval, AllowLock);
        return;
    }

    SetScanType(kScan_Interlaced, VideoOutput, m_frameInterval);
}

/** \fn MythDVDPlayer::ReleaseNextVideoFrame(VideoFrame*, int64_t)
 *  \param buffer    Buffer
 *  \param timecode  Timecode
 *  \param wrap      Ignored. This function overrides the callers
 *                   'wrap' indication and computes its own based on
 *                   whether or not video is currently playing.
 */
void MythDVDPlayer::ReleaseNextVideoFrame(MythVideoFrame *Buffer,
                                          std::chrono::milliseconds Timecode, bool /*wrap*/)
{
    MythPlayerUI::ReleaseNextVideoFrame(Buffer, Timecode,
                        !m_playerCtx->m_buffer->IsInDiscMenuOrStillFrame());
}

bool MythDVDPlayer::HasReachedEof(void) const
{
    EofState eof = GetEof();
    // DeleteMap and EditMode from the parent MythPlayer should not be
    // relevant here.
    return eof != kEofStateNone && !m_allPaused;
}

void MythDVDPlayer::DoDisableDVDSubtitles()
{
    if (kDisplayAVSubtitle == m_captionsState.m_textDisplayMode)
        SetCaptionsEnabled(false, false);
}

void MythDVDPlayer::DisableCaptions(uint Mode, bool OSDMsg)
{
    if ((kDisplayAVSubtitle & Mode) && m_playerCtx->m_buffer->IsDVD())
        m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle, -1);
    MythPlayerUI::DisableCaptions(Mode, OSDMsg);
}

void MythDVDPlayer::EnableCaptions(uint Mode, bool OSDMsg)
{
    if ((kDisplayAVSubtitle & Mode) && m_playerCtx->m_buffer->IsDVD())
    {
        int track = GetTrack(kTrackTypeSubtitle);
        if (track >= 0 && track < static_cast<int>(m_decoder->GetTrackCount(kTrackTypeSubtitle)))
        {
            StreamInfo stream = m_decoder->GetTrackInfo(kTrackTypeSubtitle, static_cast<uint>(track));
            m_playerCtx->m_buffer->DVD()->SetTrack(kTrackTypeSubtitle, stream.m_stream_id);
        }
    }
    MythPlayerUI::EnableCaptions(Mode, OSDMsg);
}

void MythDVDPlayer::DisplayPauseFrame(void)
{
    if (m_playerCtx->m_buffer->IsDVD() && m_playerCtx->m_buffer->DVD()->IsInStillFrame())
        SetScanType(kScan_Progressive, m_videoOutput, m_frameInterval);
    DisplayDVDButton();
    MythPlayerUI::DisplayPauseFrame();
}

void MythDVDPlayer::DecoderPauseCheck(void)
{
    StillFrameCheck();
    MythPlayerUI::DecoderPauseCheck();
}

bool MythDVDPlayer::PrebufferEnoughFrames(int /*MinBuffers*/)
{
    return MythPlayerUI::PrebufferEnoughFrames(1);
}

void MythDVDPlayer::DoFFRewSkip(void)
{
    MythPlayerUI::DoFFRewSkip();
    if (m_decoderChangeLock.tryLock(1))
    {
        if (m_decoder)
            m_decoder->UpdateFramesPlayed();
        m_decoderChangeLock.unlock();
    }
}

void MythDVDPlayer::PreProcessNormalFrame(void)
{
    DisplayDVDButton();
}

void MythDVDPlayer::VideoStart(void)
{
    if (!m_initialDvdState.isEmpty())
        m_playerCtx->m_buffer->DVD()->RestoreDVDStateSnapshot(m_initialDvdState);

    MythPlayerUI::VideoStart();
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
            m_videoOutput->UpdatePauseFrame(m_avSync.DisplayTimecode());

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
                LOG(VB_PLAYBACK, LOG_WARNING, LOC + "In DVD Menu: No video frames in queue");
                std::this_thread::sleep_for(10ms);
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

    return MythPlayerUI::VideoLoop();
}

bool MythDVDPlayer::FastForward(float Seconds)
{
    if (m_decoder)
        m_decoder->UpdateFramesPlayed();
    return MythPlayerUI::FastForward(Seconds);
}

bool MythDVDPlayer::Rewind(float Seconds)
{
    if (m_decoder)
        m_decoder->UpdateFramesPlayed();
    return MythPlayerUI::Rewind(Seconds);
}

bool MythDVDPlayer::JumpToFrame(uint64_t Frame)
{
    if (Frame == ~0x0ULL)
        return false;

    if (m_decoder)
        m_decoder->UpdateFramesPlayed();
    return MythPlayerUI::JumpToFrame(Frame);
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

    MythPlayerUI::EventStart();
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
            std::this_thread::sleep_for(50ms);
    }
    MythPlayerUI::InitialSeek();
    m_playerCtx->m_buffer->IgnoreWaitStates(false);
}

void MythDVDPlayer::ResetPlaying(bool /*ResetFrames*/)
{
    MythPlayerUI::ResetPlaying(false);
}

void MythDVDPlayer::EventEnd(void)
{
    if (m_playerCtx->m_buffer->DVD())
        m_playerCtx->m_buffer->DVD()->SetParent(nullptr);
}

bool MythDVDPlayer::PrepareAudioSample(std::chrono::milliseconds &Timecode)
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
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clear bookmark");
            }

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
    return frames;
}

void MythDVDPlayer::ChangeSpeed(void)
{
    if (m_stillFrameLength > 0s)
    {
        m_stillFrameTimerLock.lock();
        // Get the timestretched elapsed time and transform
        // it to what the unstretched value would have been
        // had we been playing with the new timestretch value
        // all along
        auto elapsed = millisecondsFromFloat(m_stillFrameTimer.elapsed().count() *
                                             m_playSpeed / m_nextPlaySpeed);
        m_stillFrameTimer.restart();
        m_stillFrameTimer.addMSecs(elapsed);
        m_stillFrameTimerLock.unlock();
    }

    MythPlayerUI::ChangeSpeed();

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
        m_playerCtx->m_buffer->DVD()->TitleTimeLeft() < 5s)
        return 0;
    return MythPlayerUI::CalcMaxFFTime(FastFwd, Setjump);
}

std::chrono::milliseconds MythDVDPlayer::GetMillisecondsPlayed(bool /*HonorCutList*/)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return 0ms;

    std::chrono::milliseconds played = m_playerCtx->m_buffer->DVD()->GetCurrentTime();

    if (m_stillFrameLength > 0s)
    {
        if (m_stillFrameLength == 255s)
            return -1ms;
        played = millisecondsFromFloat(m_stillFrameTimer.elapsed().count() * m_playSpeed);
    }

    return played;
}

std::chrono::milliseconds MythDVDPlayer::GetTotalMilliseconds(bool /*HonorCutList*/) const
{
    std::chrono::milliseconds total = m_totalLength;

    if (m_stillFrameLength > 0s)
    {
        if (m_stillFrameLength == 255s)
            return -1ms;
        total = duration_cast<std::chrono::milliseconds>(m_stillFrameLength);
    }

    return total;
}

void MythDVDPlayer::SetTrack(uint Type, uint TrackNo)
{
    if (kTrackTypeAudio == Type)
    {
        StreamInfo stream = m_decoder->GetTrackInfo(Type, TrackNo);
        m_playerCtx->m_buffer->DVD()->SetTrack(Type, stream.m_stream_id);
    }

    MythPlayerUI::SetTrack(Type, TrackNo);
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

void MythDVDPlayer::GetChapterTimes(QList<std::chrono::seconds> &Times)
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
            Chapter = std::max(Chapter, 0);
        }
        else if (Chapter > total)
        {
            Chapter = current + 1;
            Chapter = std::min(Chapter, total);
        }
    }

    bool success = m_playerCtx->m_buffer->DVD()->PlayTrack(Chapter);
    if (success)
    {
        if (m_decoder)
        {
            m_decoder->UpdateFramesPlayed();
            if (m_playerCtx->m_buffer->DVD()->GetCellStart() == 0s)
                m_decoder->SeekReset(static_cast<long long>(m_framesPlayed), 0, true, true);
        }
        ClearAfterSeek(!m_playerCtx->m_buffer->IsInDiscMenuOrStillFrame());
    }

    m_jumpChapter = 0;
    return success;
}

void MythDVDPlayer::DisplayDVDButton(void)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return;

    uint buttonversion = 0;
    AVSubtitle *dvdSubtitle = m_playerCtx->m_buffer->DVD()->GetMenuSubtitle(buttonversion);
    bool numbuttons    = m_playerCtx->m_buffer->DVD()->NumMenuButtons() != 0;

    bool expired = false;

    MythVideoFrame *currentFrame = m_videoOutput ? m_videoOutput->GetLastShownFrame() : nullptr;

    if (!currentFrame)
    {
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if (dvdSubtitle &&
        (dvdSubtitle->end_display_time > dvdSubtitle->start_display_time) &&
        (dvdSubtitle->end_display_time < currentFrame->m_timecode.count()))
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
        m_captionsOverlay.ClearSubtitles();
        m_buttonVersion = 0;
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if ((currentFrame->m_timecode > 0ms) &&
        (dvdSubtitle->start_display_time > currentFrame->m_timecode.count()))
    {
        m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
        return;
    }

    m_buttonVersion = static_cast<int>(buttonversion);
    QRect buttonPos = m_playerCtx->m_buffer->DVD()->GetButtonCoords();
    m_captionsOverlay.DisplayDVDButton(dvdSubtitle, buttonPos);
    uint oldcaptions = m_captionsState.m_textDisplayMode;
    m_captionsState.m_textDisplayMode = kDisplayDVDButton;
    if (oldcaptions != m_captionsState.m_textDisplayMode)
        emit CaptionsStateChanged(m_captionsState);
    m_playerCtx->m_buffer->DVD()->ReleaseMenuButton();
}

void MythDVDPlayer::GoToMenu(const QString& Menu)
{
    if (!m_playerCtx->m_buffer->IsDVD())
        return;

    uint oldcaptions = m_captionsState.m_textDisplayMode;
    m_captionsState.m_textDisplayMode = kDisplayNone;
    if (oldcaptions != m_captionsState.m_textDisplayMode)
        emit CaptionsStateChanged(m_captionsState);

    if (!m_playerCtx->m_buffer->DVD()->GoToMenu(Menu))
    {
        UpdateOSDMessage(tr("DVD Menu Not Available"), kOSDTimeout_Med);
        LOG(VB_GENERAL, LOG_ERR, "No DVD Menu available.");
    }
}

void MythDVDPlayer::GoToDVDProgram(bool Direction)
{
    if (auto * dvd = m_playerCtx->m_buffer->DVD(); dvd)
    {
        if (Direction)
            dvd->GoToPreviousProgram();
        else
            dvd->GoToNextProgram();
    }
}

bool MythDVDPlayer::IsInStillFrame() const
{
    return (m_stillFrameLength > 0s);
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
    return {};
}

bool MythDVDPlayer::SwitchAngle(int Angle)
{
    int total = GetNumAngles();
    if (!total || Angle == GetCurrentAngle())
        return false;

    if (Angle < 1 || Angle > total)
        Angle = 1;

    return m_playerCtx->m_buffer->DVD()->SwitchAngle(Angle);
}

void MythDVDPlayer::SetStillFrameTimeout(std::chrono::seconds Length)
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
       (m_stillFrameLength > 0s) && (m_stillFrameLength < 255s))
    {
        m_stillFrameTimerLock.lock();
        auto elapsedTime = secondsFromFloat(m_stillFrameTimer.elapsed().count() * m_playSpeed / 1000.0F);
        m_stillFrameTimerLock.unlock();
        if (elapsedTime >= m_stillFrameLength)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Stillframe timeout after %1 seconds (timestretch %2)")
                .arg(m_stillFrameLength.count()).arg(static_cast<double>(m_playSpeed)));
            m_playerCtx->m_buffer->DVD()->SkipStillFrame();
            m_stillFrameLength = 0s;
        }
    }
}

void MythDVDPlayer::CreateDecoder(TestBufferVec & Testbuf)
{
    if (MythDVDDecoder::CanHandle(Testbuf, m_playerCtx->m_buffer->GetFilename()))
        SetDecoder(new MythDVDDecoder(this, *m_playerCtx->m_playingInfo, m_playerFlags));
}
