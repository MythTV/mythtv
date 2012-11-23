#include "bdringbuffer.h"
#include "avformatdecoderbd.h"
#include "mythbdplayer.h"

#define LOC     QString("BDPlayer: ")

MythBDPlayer::MythBDPlayer(PlayerFlags flags)
  : MythPlayer(flags), m_stillFrameShowing(false)
{
}

void MythBDPlayer::PreProcessNormalFrame(void)
{
    DisplayMenu();
}

bool MythBDPlayer::GoToMenu(QString str)
{
    if (player_ctx->buffer->BD() && videoOutput)
    {
        int64_t pts = 0;
        VideoFrame *frame = videoOutput->GetLastShownFrame();
        if (frame)
            pts = (int64_t)(frame->timecode  * 90);
        return player_ctx->buffer->BD()->GoToMenu(str, pts);
    }
    return false;
}

void MythBDPlayer::DisplayMenu(void)
{
    if (!player_ctx->buffer->IsBD())
        return;

    osdLock.lock();
    BDOverlay *overlay = NULL;
    while (osd && (NULL != (overlay = player_ctx->buffer->BD()->GetOverlay())))
        osd->DisplayBDOverlay(overlay);
    osdLock.unlock();
}

void MythBDPlayer::DisplayPauseFrame(void)
{
    if (player_ctx->buffer->IsBD() &&
        player_ctx->buffer->BD()->IsInStillFrame())
    {
        SetScanType(kScan_Progressive);
    }
    DisplayMenu();
    MythPlayer::DisplayPauseFrame();
}

bool MythBDPlayer::VideoLoop(void)
{
    if (!player_ctx->buffer->IsBD())
    {
        SetErrored("RingBuffer is not a Blu-Ray disc.");
        return !IsErrored();
    }

    int nbframes = videoOutput ? videoOutput->ValidVideoFrames() : 0;

    // completely drain the video buffers for certain situations
    bool drain = player_ctx->buffer->BD()->BDWaitingForPlayer() &&
                 (nbframes > 0);

    if (drain)
    {
        if (nbframes < 2 && videoOutput)
            videoOutput->UpdatePauseFrame(disp_timecode);

        // if we go below the pre-buffering limit, the player will pause
        // so do this 'manually'
        DisplayNormalFrame(false);
        return !IsErrored();
    }

    // clear the mythtv imposed wait state
    if (player_ctx->buffer->BD()->BDWaitingForPlayer())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing Mythtv BD wait state");
        player_ctx->buffer->BD()->SkipBDWaitingForPlayer();
        return !IsErrored();
    }

    if (player_ctx->buffer->BD()->IsInStillFrame())
    {
        if (nbframes > 1 && !m_stillFrameShowing)
        {
            videoOutput->UpdatePauseFrame(disp_timecode);
            DisplayNormalFrame(false);
            return !IsErrored();
        }

        if (!m_stillFrameShowing)
            needNewPauseFrame = true;

        // we are in a still frame so pause video output
        if (!videoPaused)
        {
            PauseVideo();
            return !IsErrored();
        }

        // flag if we have no frame
        if (nbframes == 0)
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                    "Warning: In BD Still but no video frames in queue");
            usleep(10000);
            return !IsErrored();
        }

        if (!m_stillFrameShowing)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Entering still frame.");
        m_stillFrameShowing = true;
    }
    else
    {
        if (videoPaused && m_stillFrameShowing)
        {
            UnpauseVideo();
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Exiting still frame.");
        }
        m_stillFrameShowing = false;
    }

    return MythPlayer::VideoLoop();
}

void MythBDPlayer::EventStart(void)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        QString name;
        QString serialid;
        if (player_ctx->playingInfo->GetTitle().isEmpty() &&
            player_ctx->buffer->BD()->GetNameAndSerialNum(name, serialid))
        {
            player_ctx->playingInfo->SetTitle(name);
        }
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    MythPlayer::EventStart();
}

int MythBDPlayer::GetNumChapters(void)
{
    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen())
        return player_ctx->buffer->BD()->GetNumChapters();
    return -1;
}

int MythBDPlayer::GetCurrentChapter(void)
{
    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen())
        return player_ctx->buffer->BD()->GetCurrentChapter() + 1;
    return -1;
}

int64_t MythBDPlayer::GetChapter(int chapter)
{
    uint total = GetNumChapters();
    if (!total)
        return -1;

    return (int64_t)player_ctx->buffer->BD()->GetChapterStartFrame(chapter-1);
}

void MythBDPlayer::GetChapterTimes(QList<long long> &times)
{
    uint total = GetNumChapters();
    if (!total)
        return;

    for (uint i = 0; i < total; i++)
        times.push_back(player_ctx->buffer->BD()->GetChapterStartTime(i));
}

int MythBDPlayer::GetNumTitles(void) const
{
    if (player_ctx->buffer->BD()->IsHDMVNavigation())
        return 0;

    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen())
        return player_ctx->buffer->BD()->GetNumTitles();
    return 0;
}

int MythBDPlayer::GetNumAngles(void) const
{
    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen())
        return player_ctx->buffer->BD()->GetNumAngles();
    return 0;
}

int MythBDPlayer::GetCurrentTitle(void) const
{
    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen())
        return player_ctx->buffer->BD()->GetCurrentTitle();
    return -1;
}

int MythBDPlayer::GetCurrentAngle(void) const
{
    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen())
        return player_ctx->buffer->BD()->GetCurrentAngle();
    return -1; 
}

int MythBDPlayer::GetTitleDuration(int title) const
{
    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen() &&
        title >= 0 && title < GetNumTitles())
    {
        return player_ctx->buffer->BD()->GetTitleDuration(title);
    }
    return 0;
}

QString MythBDPlayer::GetTitleName(int title) const
{
    if (title >= 0 && title < GetNumTitles())
    {
        int secs = GetTitleDuration(title);
        // BD doesn't provide title names, so show title number and duration
        int hours = secs / 60 / 60;
        int minutes = (secs / 60) - (hours * 60);
        secs = secs % 60;
        QString name = QString("%1 (%2:%3:%4)").arg(title+1)
                .arg(hours, 2, 10, QChar(48)).arg(minutes, 2, 10, QChar(48))
                .arg(secs, 2, 10, QChar(48));
        return name;
    }
    return QString();
}

QString MythBDPlayer::GetAngleName(int angle) const
{
    if (angle >= 1 && angle <= GetNumAngles())
    {
        QString name = QObject::tr("Angle %1").arg(angle);
        return name;
    }
    return QString();
}

bool MythBDPlayer::SwitchTitle(int title)
{
    if (player_ctx->buffer->BD()->IsHDMVNavigation())
        return false;

    uint total = GetNumTitles();
    if (!total || title == GetCurrentTitle() || title >= (int)total)
        return false;

    Pause();

    bool ok = false;
    if (player_ctx->buffer->BD()->SwitchTitle(title))
    {
        ResetCaptions();
        if (OpenFile() != 0)
        {
            SetErrored(QObject::tr("Failed to switch title."));
        }
        else
        {
            ok = true;
            forcePositionMapSync = true;
        }
    }

    Play();
    return ok;
}

bool MythBDPlayer::NextTitle(void)
{
    if (player_ctx->buffer->BD()->IsHDMVNavigation())
        return false;

    uint total = GetNumTitles();
    int next = GetCurrentTitle() + 1;
    if (!total || next >= (int)total)
        return false;

    return SwitchTitle(next);
}

bool MythBDPlayer::PrevTitle(void)
{
    if (player_ctx->buffer->BD()->IsHDMVNavigation())
        return false;

    uint total = GetNumTitles();
    int prev = GetCurrentTitle() - 1;
    if (!total || prev < 0)
        return false;

    return SwitchTitle(prev);
}

bool MythBDPlayer::SwitchAngle(int angle)
{
    uint total = GetNumAngles();
    if (!total || angle == GetCurrentAngle())
        return false;

    if (angle >= (int)total)
        angle = 0;

    return player_ctx->buffer->BD()->SwitchAngle(angle);
}

bool MythBDPlayer::NextAngle(void)
{
    uint total = GetNumAngles();
    int next = GetCurrentAngle() + 1;
    if (!total)
        return false;

    if (next >= (int)total)
        next = 0;

    return SwitchAngle(next);
}

bool MythBDPlayer::PrevAngle(void)
{
    uint total = GetNumAngles();
    int prev = GetCurrentAngle() - 1;
    if (!total || total == 1)
        return false;

    if (prev < 0)
        prev = total;

    return SwitchAngle(prev);
}

void MythBDPlayer::CreateDecoder(char *testbuf, int testreadsize)
{
    if (AvFormatDecoderBD::CanHandle(testbuf, player_ctx->buffer->GetFilename(),
                                     testreadsize))
    {
        SetDecoder(new AvFormatDecoderBD(this, *player_ctx->playingInfo,
                                         playerFlags));
    }
}
