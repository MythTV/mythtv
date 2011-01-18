#include "bdringbuffer.h"
#include "mythbdplayer.h"

#define LOC     QString("BDPlayer: ")
#define LOC_ERR QString("BDPlayer error: ")

MythBDPlayer::MythBDPlayer(bool muted) : MythPlayer(muted)
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
    while (NULL != (overlay = player_ctx->buffer->BD()->GetOverlay()))
        osd->DisplayBDOverlay(overlay);
    osdLock.unlock();
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
        if (nbframes < 5 && videoOutput)
            videoOutput->UpdatePauseFrame();

        // if we go below the pre-buffering limit, the player will pause
        // so do this 'manually'
        DisplayNormalFrame(false);
        return !IsErrored();
    }

    // clear the mythtv imposed wait state
    if (player_ctx->buffer->BD()->BDWaitingForPlayer())
    {
        VERBOSE(VB_PLAYBACK, LOC + "Clearing Mythtv BD wait state");
        ClearAfterSeek(true);
        player_ctx->buffer->BD()->SkipBDWaitingForPlayer();
        return !IsErrored();
    }

    return MythPlayer::VideoLoop();
}

int MythBDPlayer::GetNumChapters(void)
{
    int num = 0;
    if (player_ctx->buffer->BD() && player_ctx->buffer->BD()->IsOpen())
        num = player_ctx->buffer->BD()->GetNumChapters();
    if (num > 1)
        return num;
    return 0;
}

int MythBDPlayer::GetCurrentChapter(void)
{
    uint total = GetNumChapters();
    if (!total)
        return 0;

    for (int i = (total - 1); i > -1 ; i--)
    {
        uint64_t frame = player_ctx->buffer->BD()->GetChapterStartFrame(i);
        if (framesPlayed >= frame)
        {
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("GetCurrentChapter(selected chapter %1 framenum %2)")
                            .arg(i + 1).arg(frame));
            return i + 1;
        }
    }
    return 0;
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
    if (angle >= 0 && angle < GetNumAngles())
    {
        QString name = QObject::tr("Angle %1").arg(angle+1);
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

