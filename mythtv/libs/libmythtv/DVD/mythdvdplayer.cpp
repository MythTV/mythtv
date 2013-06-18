#include "dvdringbuffer.h"
#include "DetectLetterbox.h"
#include "audiooutput.h"
#include "myth_imgconvert.h"
#include "avformatdecoderdvd.h"
#include "mythdvdplayer.h"

#define LOC      QString("DVDPlayer: ")

MythDVDPlayer::MythDVDPlayer(PlayerFlags flags)
  : MythPlayer(flags), m_buttonVersion(0),
    dvd_stillframe_showing(false),
    m_initial_title(-1), m_initial_audio_track(-1),
    m_initial_subtitle_track(-1),
    m_stillFrameLength(0)
{
}

void MythDVDPlayer::AutoDeint(VideoFrame *frame, bool allow_lock)
{
    (void)frame;
    (void)allow_lock;
    SetScanType(kScan_Interlaced);
}

void MythDVDPlayer::ReleaseNextVideoFrame(VideoFrame *buffer,
                                          int64_t timecode, bool wrap)
{
    MythPlayer::ReleaseNextVideoFrame(buffer, timecode,
                        !player_ctx->buffer->IsInDiscMenuOrStillFrame());
}

bool MythDVDPlayer::HasReachedEof(void) const
{
    EofState eof = GetEof();
    if (eof != kEofStateNone && !allpaused)
        return true;
    // DeleteMap and EditMode from the parent MythPlayer should not be
    // relevant here.
    return false;
}

void MythDVDPlayer::DisableCaptions(uint mode, bool osd_msg)
{
    if ((kDisplayAVSubtitle & mode) && player_ctx->buffer->IsDVD())
        player_ctx->buffer->DVD()->SetTrack(kTrackTypeSubtitle, -1);
    MythPlayer::DisableCaptions(mode, osd_msg);
}

void MythDVDPlayer::EnableCaptions(uint mode, bool osd_msg)
{
    if ((kDisplayAVSubtitle & mode) && player_ctx->buffer->IsDVD())
    {
        int track = GetTrack(kTrackTypeSubtitle);
        if (track >= 0 && track < (int)decoder->GetTrackCount(kTrackTypeSubtitle))
        {
            StreamInfo stream = decoder->GetTrackInfo(kTrackTypeSubtitle,
                                                      track);
            player_ctx->buffer->DVD()->SetTrack(kTrackTypeSubtitle,
                                                stream.stream_id);
        }
    }
    MythPlayer::EnableCaptions(mode, osd_msg);
}

void MythDVDPlayer::DisplayPauseFrame(void)
{
    if (player_ctx->buffer->IsDVD() &&
        player_ctx->buffer->DVD()->IsInStillFrame())
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

bool MythDVDPlayer::PrebufferEnoughFrames(int min_buffers)
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
    return (player_ctx->buffer->IsDVD() &&
            (player_ctx->buffer->DVD()->GetCurrentTime() < 2));
}

void MythDVDPlayer::PreProcessNormalFrame(void)
{
    DisplayDVDButton();
}

void MythDVDPlayer::VideoStart(void)
{
    if (!m_initial_dvdstate.isEmpty())
        player_ctx->buffer->DVD()->RestoreDVDStateSnapshot(m_initial_dvdstate);

    MythPlayer::VideoStart();
}

bool MythDVDPlayer::VideoLoop(void)
{
    if (!player_ctx->buffer->IsDVD())
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
    bool release_all = player_ctx->buffer->DVD()->DVDWaitingForPlayer() &&
                      (nbframes > 0);
    bool release_one = (nbframes > 1) && videoPaused && !allpaused &&
                       (!videoOutput->EnoughFreeFrames() ||
                        player_ctx->buffer->DVD()->IsWaiting() ||
                        player_ctx->buffer->DVD()->IsInStillFrame());
    if (release_all || release_one)
    {
        if (nbframes < 5 && videoOutput)
            videoOutput->UpdatePauseFrame(disp_timecode);

        // if we go below the pre-buffering limit, the player will pause
        // so do this 'manually'
        DisplayNormalFrame(false);
        // unpause the still frame if more frames become available
        if (dvd_stillframe_showing && nbframes > 1)
        {
            dvd_stillframe_showing = false;
            UnpauseVideo();
        }
        return !IsErrored();
    }

    // clear the mythtv imposed wait state
    if (player_ctx->buffer->DVD()->DVDWaitingForPlayer())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing MythTV DVD wait state");
        bool inStillFrame = player_ctx->buffer->DVD()->IsInStillFrame();
        player_ctx->buffer->DVD()->SkipDVDWaitingForPlayer();
        ClearAfterSeek(true);
        if (!inStillFrame && videoPaused && !allpaused)
            UnpauseVideo();
        return !IsErrored();
    }

    // wait for the video buffers to drain
    if (nbframes < 2)
    {
        // clear the DVD wait state
        if (player_ctx->buffer->DVD()->IsWaiting())
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing DVD wait state");
            bool inStillFrame = player_ctx->buffer->DVD()->IsInStillFrame();
            player_ctx->buffer->DVD()->WaitSkip();
            if (!inStillFrame && videoPaused && !allpaused)
                UnpauseVideo();
            return !IsErrored();
        }

        // the still frame is treated as a pause frame
        if (player_ctx->buffer->DVD()->IsInStillFrame())
        {
            // ensure we refresh the pause frame
            if (!dvd_stillframe_showing)
                needNewPauseFrame = true;

            // we are in a still frame so pause video output
            if (!videoPaused)
            {
                PauseVideo();
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

            dvd_stillframe_showing = true;
        }
        else
        {
            dvd_stillframe_showing = false;
        }
    }

    // unpause the still frame if more frames become available
    if (dvd_stillframe_showing && nbframes > 1)
    {
        UnpauseVideo();
        dvd_stillframe_showing = false;
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

    osdLock.lock();
    videofiltersLock.lock();
    videoOutput->ProcessFrame(NULL, osd, videoFilters, pip_players,
                              kScan_Progressive);
    videofiltersLock.unlock();
    osdLock.unlock();

    AVSync(NULL, true);
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
    if (decoder)
        decoder->UpdateFramesPlayed();
    return MythPlayer::JumpToFrame(frame);
}

void MythDVDPlayer::EventStart(void)
{
    if (player_ctx->buffer->DVD())
        player_ctx->buffer->DVD()->SetParent(this);

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        QString name;
        QString serialid;
        if (player_ctx->playingInfo->GetTitle().isEmpty() &&
            player_ctx->buffer->DVD() &&
            player_ctx->buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            player_ctx->playingInfo->SetTitle(name);
        }
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    MythPlayer::EventStart();
}

void MythDVDPlayer::InitialSeek(void)
{
    player_ctx->buffer->IgnoreWaitStates(true);

    if (m_initial_title > -1)
        player_ctx->buffer->DVD()->PlayTitleAndPart(m_initial_title, 1);

    if (m_initial_audio_track > -1)
        player_ctx->buffer->DVD()->SetTrack(kTrackTypeAudio,
                                            m_initial_audio_track);
    if (m_initial_subtitle_track > -1)
        player_ctx->buffer->DVD()->SetTrack(kTrackTypeSubtitle,
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
    player_ctx->buffer->IgnoreWaitStates(false);
}

void MythDVDPlayer::ResetPlaying(bool resetframes)
{
    MythPlayer::ResetPlaying(false);
}

void MythDVDPlayer::EventEnd(void)
{
    if (player_ctx->buffer->DVD())
        player_ctx->buffer->DVD()->SetParent(NULL);
}

bool MythDVDPlayer::PrepareAudioSample(int64_t &timecode)
{
    if (!player_ctx->buffer->IsInDiscMenuOrStillFrame())
        WrapTimecode(timecode, TC_AUDIO);

    if (player_ctx->buffer->IsDVD() &&
        player_ctx->buffer->DVD()->IsInStillFrame())
        return true;
    return false;
}

void MythDVDPlayer::SetBookmark(bool clear)
{
    if (!player_ctx->buffer->IsDVD())
        return;

    QStringList fields;
    QString name;
    QString serialid;
    QString dvdstate;

    if (player_ctx->buffer->IsBookmarkAllowed() || clear)
    {
        if (!player_ctx->buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "DVD has no name and serial number. Cannot set bookmark.");
            return;
        }

        if (!clear && !player_ctx->buffer->DVD()->GetDVDStateSnapshot(dvdstate))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unable to retrieve DVD state. Cannot set bookmark.");
            return;
        }

        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->playingInfo)
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

            player_ctx->playingInfo->SaveDVDBookmark(fields);

        }
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }
}

uint64_t MythDVDPlayer::GetBookmark(void)
{
    if (gCoreContext->IsDatabaseIgnored() || !player_ctx->buffer->IsDVD())
        return 0;

    QStringList dvdbookmark = QStringList();
    QString name;
    QString serialid;
    long long frames = 0;
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        if (!player_ctx->buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            return 0;
        }

        dvdbookmark = player_ctx->playingInfo->QueryDVDBookmark(serialid);

        if (!dvdbookmark.empty())
        {
            QStringList::Iterator it = dvdbookmark.begin();

            if (dvdbookmark.count() == 1)
            {
                m_initial_dvdstate = *it;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Get Bookmark: bookmark found");
            }
            else
            {
                // Legacy bookmarks
                m_initial_title = (*it).toInt();
                frames = (long long)((*++it).toLongLong() & 0xffffffffLL);
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
    MythPlayer::ChangeSpeed();
    if (decoder)
        decoder->UpdateFramesPlayed();
    if (play_speed != normal_speed && player_ctx->buffer->IsDVD())
        player_ctx->buffer->DVD()->SetDVDSpeed(-1);
    else if (player_ctx->buffer->IsDVD())
        player_ctx->buffer->DVD()->SetDVDSpeed();
}

void MythDVDPlayer::AVSync(VideoFrame *frame, bool limit_delay)
{
    MythPlayer::AVSync(frame, true);
}

long long MythDVDPlayer::CalcMaxFFTime(long long ff, bool setjump) const
{
    if ((totalFrames > 0) && player_ctx->buffer->IsDVD() &&
        player_ctx->buffer->DVD()->TitleTimeLeft() < 5)
        return 0;
    return MythPlayer::CalcMaxFFTime(ff, setjump);
}

int64_t MythDVDPlayer::GetSecondsPlayed(bool)
{
    if (!player_ctx->buffer->IsDVD())
        return 0;

    return (m_stillFrameLength > 0) ?
                (m_stillFrameTimer.elapsed() / 1000) :
                (player_ctx->buffer->DVD()->GetCurrentTime());

}

int64_t MythDVDPlayer::GetTotalSeconds(void) const
{
    return (m_stillFrameLength > 0) ? m_stillFrameLength: totalLength;
}

void MythDVDPlayer::SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                      bool absolute)
{
    if (!player_ctx->buffer->IsDVD())
        return;
    if (GoToMenu("menu"))
    {
        if (player_ctx->buffer->DVD()->IsInMenu() &&
            !player_ctx->buffer->DVD()->IsInStillFrame())
        {
            GoToDVDProgram(1);
        }
    }
    else if (player_ctx->buffer->DVD()->GetTotalTimeOfTitle() < 60)
    {
        GoToDVDProgram(1);
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
        player_ctx->buffer->DVD()->SetTrack(type, stream.stream_id);
    }

    return MythPlayer::SetTrack(type, trackNo);
}

int MythDVDPlayer::GetNumChapters(void)
{
    if (!player_ctx->buffer->IsDVD())
        return 0;
    return player_ctx->buffer->DVD()->NumPartsInTitle();
}

int MythDVDPlayer::GetCurrentChapter(void)
{
    if (!player_ctx->buffer->IsDVD())
        return 0;
    return player_ctx->buffer->DVD()->GetPart();
}

void MythDVDPlayer::GetChapterTimes(QList<long long> &times)
{
    if (!player_ctx->buffer->IsDVD())
        return;
    player_ctx->buffer->DVD()->GetChapterTimes(times);
}

bool MythDVDPlayer::DoJumpChapter(int chapter)
{
    if (!player_ctx->buffer->IsDVD())
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

    bool success = player_ctx->buffer->DVD()->playTrack(chapter);
    if (success)
    {
        if (decoder)
        {
            decoder->UpdateFramesPlayed();
            if (player_ctx->buffer->DVD()->GetCellStart() == 0)
                decoder->SeekReset(framesPlayed, 0, true, true);
        }
        ClearAfterSeek(!player_ctx->buffer->IsInDiscMenuOrStillFrame());
    }

    jumpchapter = 0;
    return success;
}

void MythDVDPlayer::DisplayDVDButton(void)
{
    if (!osd || !player_ctx->buffer->IsDVD())
        return;

    uint buttonversion = 0;
    AVSubtitle *dvdSubtitle = player_ctx->buffer->DVD()->GetMenuSubtitle(buttonversion);
    bool numbuttons    = player_ctx->buffer->DVD()->NumMenuButtons();

    bool expired = false;

    VideoFrame *currentFrame = videoOutput ? videoOutput->GetLastShownFrame() : NULL;

    if (!currentFrame)
    {
        player_ctx->buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if (dvdSubtitle &&
        (dvdSubtitle->end_display_time < currentFrame->timecode))
    {
        expired = true;
    }

    // nothing to do
    if (!expired && (buttonversion == ((uint)m_buttonVersion)))
    {
        player_ctx->buffer->DVD()->ReleaseMenuButton();
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
        player_ctx->buffer->DVD()->ReleaseMenuButton();
        return;
    }

    if (currentFrame->timecode && (dvdSubtitle->start_display_time > currentFrame->timecode))
    {
        player_ctx->buffer->DVD()->ReleaseMenuButton();
        return;
    }

    m_buttonVersion = buttonversion;
    QRect buttonPos = player_ctx->buffer->DVD()->GetButtonCoords();
    osdLock.lock();
    if (osd)
        osd->DisplayDVDButton(dvdSubtitle, buttonPos);
    osdLock.unlock();
    textDisplayMode = kDisplayDVDButton;
    player_ctx->buffer->DVD()->ReleaseMenuButton();
}

bool MythDVDPlayer::GoToMenu(QString str)
{
    if (!player_ctx->buffer->IsDVD())
        return false;
    textDisplayMode = kDisplayNone;
    bool ret = player_ctx->buffer->DVD()->GoToMenu(str);

    if (!ret)
    {
        SetOSDMessage(QObject::tr("DVD Menu Not Available"), kOSDTimeout_Med);
        LOG(VB_GENERAL, LOG_ERR, "No DVD Menu available.");
        return false;
    }

    return true;
}

void MythDVDPlayer::GoToDVDProgram(bool direction)
{
    if (!player_ctx->buffer->IsDVD())
        return;
    if (direction == 0)
        player_ctx->buffer->DVD()->GoToPreviousProgram();
    else
        player_ctx->buffer->DVD()->GoToNextProgram();
}

int MythDVDPlayer::GetNumAngles(void) const
{
    if (player_ctx->buffer->DVD() && player_ctx->buffer->DVD()->IsOpen())
        return player_ctx->buffer->DVD()->GetNumAngles();
    return 0;
}

int MythDVDPlayer::GetCurrentAngle(void) const
{
    if (player_ctx->buffer->DVD() && player_ctx->buffer->DVD()->IsOpen())
        return player_ctx->buffer->DVD()->GetCurrentAngle();
    return -1; 
}

QString MythDVDPlayer::GetAngleName(int angle) const
{
    if (angle >= 1 && angle <= GetNumAngles())
    {
        QString name = QObject::tr("Angle %1").arg(angle);
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

    return player_ctx->buffer->DVD()->SwitchAngle(angle);
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
    if (player_ctx->buffer->IsDVD() &&
        player_ctx->buffer->DVD()->IsInStillFrame() &&
       (m_stillFrameLength > 0) && (m_stillFrameLength < 0xff))
    {
        m_stillFrameTimerLock.lock();
        int elapsedTime = m_stillFrameTimer.elapsed() / 1000;
        m_stillFrameTimerLock.unlock();
        if (elapsedTime >= m_stillFrameLength)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Stillframe timeout after %1 seconds")
                    .arg(m_stillFrameLength));
            player_ctx->buffer->DVD()->SkipStillFrame();
            m_stillFrameLength = 0;
        }
    }
}

void MythDVDPlayer::CreateDecoder(char *testbuf, int testreadsize)
{
    if (AvFormatDecoderDVD::CanHandle(testbuf, player_ctx->buffer->GetFilename(),
                                      testreadsize))
    {
        SetDecoder(new AvFormatDecoderDVD(this, *player_ctx->playingInfo,
                                          playerFlags));
    }
}
