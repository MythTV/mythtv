// MythTV
#include "tv_play.h"
#include "Bluray/mythbdbuffer.h"
#include "Bluray/mythbddecoder.h"
#include "Bluray/mythbdplayer.h"

#include "libmythbase/mythdate.h"

// Std
#include <unistd.h>

#define LOC QString("BDPlayer: ")

MythBDPlayer::MythBDPlayer(MythMainWindow *MainWindow, TV *Tv, PlayerContext *Context, PlayerFlags Flags)
  : MythPlayerUI(MainWindow, Tv, Context, Flags)
{
    connect(Tv, &TV::GoToMenu, this, &MythBDPlayer::GoToMenu);
}

bool MythBDPlayer::HasReachedEof(void) const
{
    EofState eof = GetEof();
    // DeleteMap and EditMode from the parent MythPlayer should not be
    // relevant here.
    return eof != kEofStateNone && !m_allPaused;
}

void MythBDPlayer::PreProcessNormalFrame(void)
{
    DisplayMenu();
}

void MythBDPlayer::GoToMenu(const QString& Menu)
{
    if (!(m_playerCtx->m_buffer->BD() && m_videoOutput))
        return;

    mpeg::chrono::pts pts = 0_pts;
    const auto * frame = m_videoOutput->GetLastShownFrame();
    if (frame)
        pts = duration_cast<mpeg::chrono::pts>(frame->m_timecode);
    m_playerCtx->m_buffer->BD()->GoToMenu(Menu, pts);
}

void MythBDPlayer::DisplayMenu(void)
{
    if (!m_playerCtx->m_buffer->IsBD())
        return;

    MythBDOverlay *overlay = nullptr;
    while (nullptr != (overlay = m_playerCtx->m_buffer->BD()->GetOverlay()))
        m_captionsOverlay.DisplayBDOverlay(overlay);
}

void MythBDPlayer::DisplayPauseFrame(void)
{
    if (m_playerCtx->m_buffer->IsBD() && m_playerCtx->m_buffer->BD()->IsInStillFrame())
        SetScanType(kScan_Progressive, m_videoOutput, m_frameInterval);
    DisplayMenu();
    MythPlayerUI::DisplayPauseFrame();
}

void MythBDPlayer::VideoStart(void)
{
    if (!m_initialBDState.isEmpty())
        m_playerCtx->m_buffer->BD()->RestoreBDStateSnapshot(m_initialBDState);

    MythPlayerUI::VideoStart();
}

bool MythBDPlayer::VideoLoop(void)
{
    if (!m_playerCtx->m_buffer->IsBD())
    {
        SetErrored("RingBuffer is not a Blu-Ray disc.");
        return !IsErrored();
    }

    int nbframes = m_videoOutput ? m_videoOutput->ValidVideoFrames() : 0;

    // completely drain the video buffers for certain situations
    if (m_playerCtx->m_buffer->BD()->BDWaitingForPlayer() && (nbframes > 0))
    {
        if (nbframes < 2 && m_videoOutput)
            m_videoOutput->UpdatePauseFrame(m_avSync.DisplayTimecode());

        // if we go below the pre-buffering limit, the player will pause
        // so do this 'manually'
        DisplayNormalFrame(false);
        return !IsErrored();
    }

    // clear the mythtv imposed wait state
    if (m_playerCtx->m_buffer->BD()->BDWaitingForPlayer())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clearing Mythtv BD wait state");
        m_playerCtx->m_buffer->BD()->SkipBDWaitingForPlayer();
        return !IsErrored();
    }

    if (m_playerCtx->m_buffer->BD()->IsInStillFrame())
    {
        if (nbframes > 1 && !m_stillFrameShowing)
        {
            m_videoOutput->UpdatePauseFrame(m_avSync.DisplayTimecode());
            DisplayNormalFrame(false);
            return !IsErrored();
        }

        if (!m_stillFrameShowing)
            m_needNewPauseFrame = true;

        // we are in a still frame so pause video output
        if (!m_videoPaused)
        {
            PauseVideo();
            return !IsErrored();
        }

        // flag if we have no frame
        if (nbframes == 0)
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC + "Warning: In BD Still but no video frames in queue");
            std::this_thread::sleep_for(10ms);
            return !IsErrored();
        }

        if (!m_stillFrameShowing)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Entering still frame.");
        m_stillFrameShowing = true;
    }
    else
    {
        if (m_videoPaused && m_stillFrameShowing)
        {
            UnpauseVideo();
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Exiting still frame.");
        }
        m_stillFrameShowing = false;
    }

    return MythPlayerUI::VideoLoop();
}

bool MythBDPlayer::JumpToFrame(uint64_t Frame)
{
    if (Frame == ~0x0ULL)
        return false;
    return MythPlayerUI::JumpToFrame(Frame);
}

void MythBDPlayer::EventStart(void)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
    {
        QString name;
        QString serialid;
        if (m_playerCtx->m_playingInfo->GetTitle().isEmpty() &&
            m_playerCtx->m_buffer->BD()->GetNameAndSerialNum(name, serialid))
        {
            m_playerCtx->m_playingInfo->SetTitle(name);
        }
    }
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    MythPlayerUI::EventStart();
}

int MythBDPlayer::GetNumChapters(void)
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return static_cast<int>(m_playerCtx->m_buffer->BD()->GetNumChapters());
    return -1;
}

int MythBDPlayer::GetCurrentChapter(void)
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return static_cast<int>(m_playerCtx->m_buffer->BD()->GetCurrentChapter() + 1);
    return -1;
}

int64_t MythBDPlayer::GetChapter(int Chapter)
{
    if (GetNumChapters() < 1)
        return -1;
    auto chapter = static_cast<uint32_t>(Chapter - 1);
    return static_cast<int64_t>(m_playerCtx->m_buffer->BD()->GetChapterStartFrame(chapter));
}

void MythBDPlayer::GetChapterTimes(QList<std::chrono::seconds> &ChapterTimes)
{
    uint total = static_cast<uint>(GetNumChapters());
    for (uint i = 0; i < total; i++)
        ChapterTimes.push_back(m_playerCtx->m_buffer->BD()->GetChapterStartTime(i));
}

int MythBDPlayer::GetNumTitles(void) const
{
    if (m_playerCtx->m_buffer->BD()->IsHDMVNavigation())
        return 0;

    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return static_cast<int>(m_playerCtx->m_buffer->BD()->GetNumTitles());
    return 0;
}

int MythBDPlayer::GetNumAngles(void) const
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return static_cast<int>(m_playerCtx->m_buffer->BD()->GetNumAngles());
    return 0;
}

int MythBDPlayer::GetCurrentTitle(void) const
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return m_playerCtx->m_buffer->BD()->GetCurrentTitle();
    return -1;
}

int MythBDPlayer::GetCurrentAngle(void) const
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return static_cast<int>(m_playerCtx->m_buffer->BD()->GetCurrentAngle());
    return -1;
}

std::chrono::seconds MythBDPlayer::GetTitleDuration(int Title) const
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen() &&
        Title >= 0 && Title < GetNumTitles())
    {
        return m_playerCtx->m_buffer->BD()->GetTitleDuration(Title);
    }
    return 0s;
}

QString MythBDPlayer::GetTitleName(int Title) const
{
    if (Title >= 0 && Title < GetNumTitles())
    {
        // BD doesn't provide title names, so show title number and duration
        QString timestr = MythDate::formatTime(GetTitleDuration(Title), "HH:mm:ss");
        QString name = QString("%1 (%2)").arg(Title+1).arg(timestr);
        return name;
    }
    return {};
}

QString MythBDPlayer::GetAngleName(int Angle) const
{
    if (Angle >= 1 && Angle <= GetNumAngles())
        return tr("Angle %1").arg(Angle);
    return {};
}

bool MythBDPlayer::SwitchTitle(int Title)
{
    if (m_playerCtx->m_buffer->BD()->IsHDMVNavigation())
        return false;

    int total = GetNumTitles();
    if ((total < 1) || Title == GetCurrentTitle() || (Title >= total))
        return false;

    Pause();

    bool ok = false;
    if (m_playerCtx->m_buffer->BD()->SwitchTitle(static_cast<uint32_t>(Title)))
    {
        ResetCaptions();
        if (OpenFile() != 0)
        {
            SetErrored(tr("Failed to switch title."));
        }
        else
        {
            ok = true;
            m_forcePositionMapSync = true;
        }
    }

    Play();
    return ok;
}

bool MythBDPlayer::NextTitle(void)
{
    if (m_playerCtx->m_buffer->BD()->IsHDMVNavigation())
        return false;

    int total = GetNumTitles();
    int next = GetCurrentTitle() + 1;
    if ((total < 1) || (next >= total))
        return false;

    return SwitchTitle(next);
}

bool MythBDPlayer::PrevTitle(void)
{
    if (m_playerCtx->m_buffer->BD()->IsHDMVNavigation())
        return false;

    uint total = static_cast<uint>(GetNumTitles());
    int prev = GetCurrentTitle() - 1;
    if (!total || prev < 0)
        return false;

    return SwitchTitle(prev);
}

bool MythBDPlayer::SwitchAngle(int Angle)
{
    int total = GetNumAngles();
    if (!total || Angle == GetCurrentAngle())
        return false;

    if (Angle >= total)
        Angle = 0;

    return m_playerCtx->m_buffer->BD()->SwitchAngle(static_cast<uint>(Angle));
}

bool MythBDPlayer::NextAngle(void)
{
    int total = GetNumAngles();
    int next = GetCurrentAngle() + 1;
    if (total < 1)
        return false;
    if (next >= total)
        next = 0;
    return SwitchAngle(next);
}

bool MythBDPlayer::PrevAngle(void)
{
    int total = GetNumAngles();
    int prev = GetCurrentAngle() - 1;
    if ((total < 1) || total == 1)
        return false;
    if (prev < 0)
        prev = total;
    return SwitchAngle(prev);
}

void MythBDPlayer::SetBookmark(bool Clear)
{
    if (!m_playerCtx->m_buffer->IsInMenu() && (m_playerCtx->m_buffer->IsBookmarkAllowed() || Clear))
    {
        QString name;
        QString serialid;
        if (!m_playerCtx->m_buffer->BD()->GetNameAndSerialNum(name, serialid))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "BD has no name and serial number. Cannot set bookmark.");
            return;
        }

        QString bdstate;
        if (!Clear && !m_playerCtx->m_buffer->BD()->GetBDStateSnapshot(bdstate))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to retrieve BD state. Cannot set bookmark.");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("BDState:%1").arg(bdstate));

        m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
        if (m_playerCtx->m_playingInfo)
        {
            QStringList fields;
            fields += serialid;
            fields += name;

            if (!Clear)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Set bookmark");
                fields += bdstate;
            }
            else
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clear bookmark");
            }

            ProgramInfo::SaveBDBookmark(fields);
        }
        m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    }
}

uint64_t MythBDPlayer::GetBookmark(void)
{
    uint64_t frames = 0;
    if (gCoreContext->IsDatabaseIgnored() || !m_playerCtx->m_buffer->IsBD())
        return frames;

    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);

    if (m_playerCtx->m_playingInfo)
    {
        QString name;
        QString serialid;
        if (!m_playerCtx->m_buffer->BD()->GetNameAndSerialNum(name, serialid))
        {
            m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
            return frames;
        }

        QStringList bdbookmark = m_playerCtx->m_playingInfo->QueryBDBookmark(serialid);

        if (!bdbookmark.empty())
        {
            m_initialBDState = bdbookmark[0];
            frames = ~0x0ULL;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Get Bookmark: bookmark found");
        }
    }

    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    return frames;
}

void MythBDPlayer::CreateDecoder(TestBufferVec & TestBuffer)
{
    if (MythBDDecoder::CanHandle(TestBuffer, m_playerCtx->m_buffer->GetFilename()))
        SetDecoder(new MythBDDecoder(this, *m_playerCtx->m_playingInfo, m_playerFlags));
}
