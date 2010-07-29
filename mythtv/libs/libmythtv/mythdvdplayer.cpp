#include "DVDRingBuffer.h"
#include "DetectLetterbox.h"
#include "audiooutput.h"
#include "myth_imgconvert.h"
#include "mythdvdplayer.h"

#define LOC      QString("DVDPlayer: ")
#define LOC_WARN QString("DVDPlayer, Warning: ")
#define LOC_ERR  QString("DVDPlayer, Error: ")

MythDVDPlayer::MythDVDPlayer(bool muted)
  : NuppelVideoPlayer(muted), hidedvdbutton(true),
    dvd_stillframe_showing(false), need_change_dvd_track(0)
{
}

void MythDVDPlayer::AutoDeint(VideoFrame *frame, bool allow_lock)
{
    NuppelVideoPlayer::AutoDeint(frame, false);
}

void MythDVDPlayer::ReleaseNextVideoFrame(VideoFrame *buffer,
                                          long long timecode, bool wrap)
{
    NuppelVideoPlayer::ReleaseNextVideoFrame(buffer, timecode,
                        !player_ctx->buffer->InDVDMenuOrStillFrame());
}

int MythDVDPlayer::OpenFile(uint retries, bool allow_libmpeg2)
{
    NuppelVideoPlayer::OpenFile(retries, allow_libmpeg2);
    if (player_ctx->buffer->isDVD())
        player_ctx->buffer->DVD()->JumpToTitle(true);
    return IsErrored() ? -1 : 0;
}

void MythDVDPlayer::DisableCaptions(uint mode, bool osd_msg)
{
    if ((kDisplayAVSubtitle & mode) && player_ctx->buffer->isDVD())
        player_ctx->buffer->DVD()->SetTrack(kTrackTypeSubtitle, -1);
    NuppelVideoPlayer::DisableCaptions(mode, osd_msg);
}

void MythDVDPlayer::EnableCaptions(uint mode, bool osd_msg)
{
    if ((kDisplayAVSubtitle & mode) && player_ctx->buffer->isDVD())
        player_ctx->buffer->DVD()->SetTrack(kTrackTypeSubtitle,
                                            GetTrack(kTrackTypeSubtitle));
    NuppelVideoPlayer::EnableCaptions(mode, osd_msg);
}

void MythDVDPlayer::DisplayPauseFrame(void)
{
    if (player_ctx->buffer->isDVD() &&
        player_ctx->buffer->DVD()->InStillFrame())
        SetScanType(kScan_Progressive);
    DisplayDVDButton();
    NuppelVideoPlayer::DisplayPauseFrame();
}

bool MythDVDPlayer::PrebufferEnoughFrames(bool pause_audio, int  min_buffers)
{
    bool instill = player_ctx->buffer->InDVDMenuOrStillFrame();
    return NuppelVideoPlayer::PrebufferEnoughFrames(!instill, (int)instill);
}

bool MythDVDPlayer::DecoderGetFrameFFREW(void)
{
    if (GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();
    return NuppelVideoPlayer::DecoderGetFrameFFREW();
}

bool MythDVDPlayer::DecoderGetFrameREW(void)
{
    NuppelVideoPlayer::DecoderGetFrameREW();
    return (player_ctx->buffer->isDVD() &&
           (player_ctx->buffer->DVD()->GetCurrentTime() < 2));
}

void MythDVDPlayer::DisplayNormalFrame(bool allow_pause)
{
    bool allow = player_ctx->buffer->isDVD() &&
                (!player_ctx->buffer->InDVDMenuOrStillFrame() ||
                (player_ctx->buffer->DVD()->NumMenuButtons() > 0 &&
                 player_ctx->buffer->DVD()->GetChapterLength() > 3));
    NuppelVideoPlayer::DisplayNormalFrame(allow);
}

void MythDVDPlayer::PreProcessNormalFrame(void)
{
    DisplayDVDButton();
}

bool MythDVDPlayer::VideoLoop(void)
{
    int nbframes = 0;
    if (videoOutput)
        nbframes = videoOutput->ValidVideoFrames();

    if (nbframes < 2 && player_ctx->buffer->isDVD())
    {
        bool isWaiting  = player_ctx->buffer->DVD()->IsWaiting();

        if (isWaiting)
        {
            player_ctx->buffer->DVD()->WaitSkip();
            return !IsErrored();
        }

        if (player_ctx->buffer->InDVDMenuOrStillFrame())
        {
            if (nbframes == 0)
            {
                VERBOSE(VB_PLAYBACK+VB_EXTRA, LOC_ERR +
                        "In DVD Menu: No video frames in queue");
                if (videoPaused)
                    UnpauseVideo();
                usleep(10000);
                return !IsErrored();
            }

            if (!videoPaused && nbframes == 1)
            {
                dvd_stillframe_showing = true;
                PauseVideo();
            }
        }
    }

    if (dvd_stillframe_showing && nbframes > 1)
    {
        UnpauseVideo();
        dvd_stillframe_showing = false;
        return !IsErrored();
    }

    if (videoPaused || isDummy)
    {
        usleep(frame_interval);
        DisplayPauseFrame();
    }
    else
        DisplayNormalFrame();

    if (using_null_videoout)
        GetDecoder()->UpdateFramesPlayed();
    else
        framesPlayed = videoOutput->GetFramesPlayed();
    return !IsErrored();
}

bool MythDVDPlayer::FastForward(float seconds)
{
    if (GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();
    return NuppelVideoPlayer::FastForward(seconds);
}

bool MythDVDPlayer::Rewind(float seconds)
{
    if (GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();
    return NuppelVideoPlayer::Rewind(seconds);
}

bool MythDVDPlayer::JumpToFrame(uint64_t frame)
{
    if (GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();
    return NuppelVideoPlayer::JumpToFrame(frame);
}

void MythDVDPlayer::EventStart(void)
{
    player_ctx->buffer->DVD()->SetParent(this);
    NuppelVideoPlayer::EventStart();
}

void MythDVDPlayer::EventLoop(void)
{
    NuppelVideoPlayer::EventLoop();
    if (need_change_dvd_track)
    {
        DoChangeDVDTrack();
        need_change_dvd_track = 0;
    }
}

void MythDVDPlayer::ResetPlaying(bool resetframes)
{
    NuppelVideoPlayer::ResetPlaying(false);
}

void MythDVDPlayer::EventEnd(void)
{
    player_ctx->buffer->DVD()->SetParent(NULL);
}

bool MythDVDPlayer::PrepareAudioSample(long long &timecode)
{
    if (!player_ctx->buffer->InDVDMenuOrStillFrame())
        WrapTimecode(timecode, TC_AUDIO);

    if (player_ctx->buffer->isDVD() &&
        player_ctx->buffer->DVD()->InStillFrame())
        return true;
    return false;
}

void MythDVDPlayer::SetBookmark(void)
{
    if (player_ctx->buffer->InDVDMenuOrStillFrame())
        SetDVDBookmark(0);
    else
    {
        SetDVDBookmark(framesPlayed);
        SetOSDStatus(QObject::tr("Position"), kOSDTimeout_Med);
        SetOSDMessage(QObject::tr("Bookmark Saved"), kOSDTimeout_Med);
    }
}

void MythDVDPlayer::ClearBookmark(bool message)
{
    SetDVDBookmark(0);
    if (message)
        SetOSDMessage(QObject::tr("Bookmark Cleared"), kOSDTimeout_Med);
}

uint64_t MythDVDPlayer::GetBookmark(void) const
{
    if (gCoreContext->IsDatabaseIgnored() || !player_ctx->buffer->isDVD())
        return 0;

    QStringList dvdbookmark = QStringList();
    QString name;
    QString serialid;
    long long frames = 0;
    bool delbookmark, jumptotitle;
    delbookmark = jumptotitle = player_ctx->buffer->DVD()->JumpToTitle();
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        if (!player_ctx->buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            return 0;
        }
        dvdbookmark = player_ctx->playingInfo->QueryDVDBookmark(serialid,
                                                                !delbookmark);
        if (!dvdbookmark.empty())
        {
            QStringList::Iterator it = dvdbookmark.begin();
            int title = (*it).toInt();
            frames = (long long)((*++it).toLongLong() & 0xffffffffLL);
            if (jumptotitle)
            {
                player_ctx->buffer->DVD()->PlayTitleAndPart(title, 1);
                int audiotrack    = (*++it).toInt();
                int subtitletrack = (*++it).toInt();
                player_ctx->buffer->DVD()->SetTrack(
                    kTrackTypeAudio, audiotrack);
                player_ctx->buffer->DVD()->SetTrack(
                    kTrackTypeSubtitle, subtitletrack);
                player_ctx->buffer->DVD()->JumpToTitle(false);
            }
        }
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    return frames;;
}

void MythDVDPlayer::ChangeSpeed(void)
{
    NuppelVideoPlayer::ChangeSpeed();
    if (GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();
    if (play_speed != normal_speed && player_ctx->buffer->isDVD())
        player_ctx->buffer->DVD()->SetDVDSpeed(-1);
    else if (player_ctx->buffer->isDVD())
        player_ctx->buffer->DVD()->SetDVDSpeed();
}

void MythDVDPlayer::AVSync(bool limit_delay)
{
    NuppelVideoPlayer::AVSync(true);
}

long long MythDVDPlayer::CalcMaxFFTime(long long ff, bool setjump) const
{
    if ((totalFrames > 0) && player_ctx->buffer->isDVD() &&
        player_ctx->buffer->DVD()->TitleTimeLeft() < 5)
        return 0;
    return NuppelVideoPlayer::CalcMaxFFTime(ff, setjump);
}

void MythDVDPlayer::calcSliderPos(osdInfo &info, bool paddedFields)
{
    bool islive = false;
    info.text.insert("description", "");
    info.values.insert("position",   0);
    info.values.insert("progbefore", 0);
    info.values.insert("progafter",  0);
    if (!player_ctx->buffer->isDVD())
        return;
    if (player_ctx->buffer->DVD()->IsInMenu())
    {
        long long rPos = player_ctx->buffer->GetReadPosition();
        long long tPos = 1;//player_ctx->buffer->GetTotalReadPosition();

        player_ctx->buffer->DVD()->GetDescForPos(info.text["description"]);

        if (rPos)
            info.values["position"] = (int)((rPos / tPos) * 1000.0);

        return;
    }

    int playbackLen = totalLength;
    float secsplayed = 0.0f;
    if (!player_ctx->buffer->DVD()->IsInMenu())
#if !CONFIG_CYGWIN
        secsplayed = player_ctx->buffer->DVD()->GetCurrentTime();
#else
        // DVD playing non-functional under windows for now
        secsplayed = 0.0f;
#endif
    calcSliderPosPriv(info, paddedFields, playbackLen, secsplayed, islive);
}

void MythDVDPlayer::SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                      bool absolute)
{
    if (!player_ctx->buffer->isDVD())
        return;
    if (GoToDVDMenu("menu"))
    {
        if (player_ctx->buffer->DVD()->IsInMenu() &&
           !player_ctx->buffer->DVD()->InStillFrame())
            GoToDVDProgram(1);
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
        player_ctx->buffer->DVD()->SetTrack(type, trackNo);
    return NuppelVideoPlayer::SetTrack(type, trackNo);
}

void MythDVDPlayer::ChangeDVDTrack(bool ffw)
{
    need_change_dvd_track = (ffw ? 1 : -1);
}

void MythDVDPlayer::DoChangeDVDTrack(void)
{
    SaveAudioTimecodeOffset(GetAudioTimecodeOffset());
    GetDecoder()->ChangeDVDTrack(need_change_dvd_track > 0);
    ClearAfterSeek(!player_ctx->buffer->InDVDMenuOrStillFrame());
}

void MythDVDPlayer::DisplayDVDButton(void)
{
    if (!osd || !player_ctx->buffer->isDVD())
        return;

    VideoFrame *buffer = videoOutput->GetLastShownFrame();

    bool numbuttons = player_ctx->buffer->DVD()->NumMenuButtons();
    bool osdshown   = osd->IsWindowVisible(OSD_WIN_SUBTITLE);

    if ((!numbuttons) ||
        (osdshown) ||
        (dvd_stillframe_showing && buffer->timecode > 0) ||
        ((!osdshown) &&
            (!videoPaused) &&
            (hidedvdbutton) &&
            (buffer->timecode > 0)))
    {
        return;
    }

    AVSubtitle *dvdSubtitle = player_ctx->buffer->DVD()->GetMenuSubtitle();
    QRect buttonPos = player_ctx->buffer->DVD()->GetButtonCoords();
    if (dvdSubtitle)
        osd->DisplayDVDButton(dvdSubtitle, buttonPos);
    player_ctx->buffer->DVD()->ReleaseMenuButton();
    hidedvdbutton = false;
}

bool MythDVDPlayer::GoToDVDMenu(QString str)
{
    if (!player_ctx->buffer->isDVD())
        return false;
    textDisplayMode = kDisplayNone;
    bool ret = player_ctx->buffer->DVD()->GoToMenu(str);

    if (!ret)
    {
        SetOSDMessage(QObject::tr("DVD Menu Not Available"), kOSDTimeout_Med);
        VERBOSE(VB_GENERAL, "No DVD Menu available.");
        return false;
    }

    return true;
}

void MythDVDPlayer::GoToDVDProgram(bool direction)
{
    if (!player_ctx->buffer->isDVD())
        return;
    if (direction == 0)
        player_ctx->buffer->DVD()->GoToPreviousProgram();
    else
        player_ctx->buffer->DVD()->GoToNextProgram();
}

void MythDVDPlayer::SetDVDBookmark(uint64_t frame)
{
    if (!player_ctx->buffer->isDVD())
        return;
    uint64_t framenum = frame;
    QStringList fields;
    QString name;
    QString serialid;
    int title = 0;
    int part;
    int audiotrack = -1;
    int subtitletrack = -1;
    if (!player_ctx->buffer->DVD()->GetNameAndSerialNum(name, serialid))
        return;

    if (!player_ctx->buffer->InDVDMenuOrStillFrame() &&
        player_ctx->buffer->DVD()->GetTotalTimeOfTitle() > 120 && frame > 0)
    {
        audiotrack = GetTrack(kTrackTypeAudio);
        if (GetCaptionMode() == kDisplayAVSubtitle)
        {
            subtitletrack = player_ctx->buffer->DVD()->GetTrack(
                kTrackTypeSubtitle);
        }
        player_ctx->buffer->DVD()->GetPartAndTitle(part, title);
    }
    else
        framenum = 0;

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        fields += serialid;
        fields += name;
        fields += QString("%1").arg(title);
        fields += QString("%1").arg(audiotrack);
        fields += QString("%1").arg(subtitletrack);
        fields += QString("%1").arg(framenum);
        player_ctx->playingInfo->SaveDVDBookmark(fields);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}
