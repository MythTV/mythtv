#include "bdringbuffer.h"
#include "avformatdecoderbd.h"
#include "mythbdplayer.h"

#include <unistd.h> // for usleep()

#define LOC     QString("BDPlayer: ")

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

bool MythBDPlayer::GoToMenu(QString str)
{
    if (m_playerCtx->m_buffer->BD() && m_videoOutput)
    {
        int64_t pts = 0;
        VideoFrame *frame = m_videoOutput->GetLastShownFrame();
        if (frame)
            pts = (int64_t)(frame->timecode  * 90);
        return m_playerCtx->m_buffer->BD()->GoToMenu(str, pts);
    }
    return false;
}

void MythBDPlayer::DisplayMenu(void)
{
    if (!m_playerCtx->m_buffer->IsBD())
        return;

    m_osdLock.lock();
    BDOverlay *overlay = nullptr;
    while (m_osd && (nullptr != (overlay = m_playerCtx->m_buffer->BD()->GetOverlay())))
        m_osd->DisplayBDOverlay(overlay);
    m_osdLock.unlock();
}

void MythBDPlayer::DisplayPauseFrame(void)
{
    if (m_playerCtx->m_buffer->IsBD() &&
        m_playerCtx->m_buffer->BD()->IsInStillFrame())
    {
        SetScanType(kScan_Progressive);
    }
    DisplayMenu();
    MythPlayer::DisplayPauseFrame();
}

void MythBDPlayer::VideoStart(void)
{
    if (!m_initialBDState.isEmpty())
        m_playerCtx->m_buffer->BD()->RestoreBDStateSnapshot(m_initialBDState);

    MythPlayer::VideoStart();
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
    bool drain = m_playerCtx->m_buffer->BD()->BDWaitingForPlayer() &&
                 (nbframes > 0);

    if (drain)
    {
        if (nbframes < 2 && m_videoOutput)
            m_videoOutput->UpdatePauseFrame(m_dispTimecode);

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
            m_videoOutput->UpdatePauseFrame(m_dispTimecode);
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
        if (m_videoPaused && m_stillFrameShowing)
        {
            UnpauseVideo();
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Exiting still frame.");
        }
        m_stillFrameShowing = false;
    }

    return MythPlayer::VideoLoop();
}

bool MythBDPlayer::JumpToFrame(uint64_t frame)
{
    if (frame == ~0x0ULL)
        return false;

    return MythPlayer::JumpToFrame(frame);
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

    MythPlayer::EventStart();
}

int MythBDPlayer::GetNumChapters(void)
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return m_playerCtx->m_buffer->BD()->GetNumChapters();
    return -1;
}

int MythBDPlayer::GetCurrentChapter(void)
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return m_playerCtx->m_buffer->BD()->GetCurrentChapter() + 1;
    return -1;
}

int64_t MythBDPlayer::GetChapter(int chapter)
{
    uint total = GetNumChapters();
    if (!total)
        return -1;

    return (int64_t)m_playerCtx->m_buffer->BD()->GetChapterStartFrame(chapter-1);
}

void MythBDPlayer::GetChapterTimes(QList<long long> &times)
{
    uint total = GetNumChapters();
    if (!total)
        return;

    for (uint i = 0; i < total; i++)
        times.push_back(m_playerCtx->m_buffer->BD()->GetChapterStartTime(i));
}

int MythBDPlayer::GetNumTitles(void) const
{
    if (m_playerCtx->m_buffer->BD()->IsHDMVNavigation())
        return 0;

    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return m_playerCtx->m_buffer->BD()->GetNumTitles();
    return 0;
}

int MythBDPlayer::GetNumAngles(void) const
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen())
        return m_playerCtx->m_buffer->BD()->GetNumAngles();
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
        return m_playerCtx->m_buffer->BD()->GetCurrentAngle();
    return -1;
}

int MythBDPlayer::GetTitleDuration(int title) const
{
    if (m_playerCtx->m_buffer->BD() && m_playerCtx->m_buffer->BD()->IsOpen() &&
        title >= 0 && title < GetNumTitles())
    {
        return m_playerCtx->m_buffer->BD()->GetTitleDuration(title);
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
        QString name = tr("Angle %1").arg(angle);
        return name;
    }
    return QString();
}

bool MythBDPlayer::SwitchTitle(int title)
{
    if (m_playerCtx->m_buffer->BD()->IsHDMVNavigation())
        return false;

    uint total = GetNumTitles();
    if (!total || title == GetCurrentTitle() || title >= (int)total)
        return false;

    Pause();

    bool ok = false;
    if (m_playerCtx->m_buffer->BD()->SwitchTitle(title))
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

    uint total = GetNumTitles();
    int next = GetCurrentTitle() + 1;
    if (!total || next >= (int)total)
        return false;

    return SwitchTitle(next);
}

bool MythBDPlayer::PrevTitle(void)
{
    if (m_playerCtx->m_buffer->BD()->IsHDMVNavigation())
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

    return m_playerCtx->m_buffer->BD()->SwitchAngle(angle);
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

void MythBDPlayer::SetBookmark(bool clear)
{
    QStringList fields;
    QString name;
    QString serialid;
    QString bdstate;

    if (!m_playerCtx->m_buffer->IsInMenu() &&
        (m_playerCtx->m_buffer->IsBookmarkAllowed() || clear))
    {
        if (!m_playerCtx->m_buffer->BD()->GetNameAndSerialNum(name, serialid))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "BD has no name and serial number. Cannot set bookmark.");
            return;
        }

        if (!clear && !m_playerCtx->m_buffer->BD()->GetBDStateSnapshot(bdstate))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unable to retrieve BD state. Cannot set bookmark.");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("BDState:%1").arg(bdstate));

        m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
        if (m_playerCtx->m_playingInfo)
        {
            fields += serialid;
            fields += name;

            if (!clear)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Set bookmark");
                fields += bdstate;
            }
            else
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Clear bookmark");

            ProgramInfo::SaveBDBookmark(fields);

        }
        m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    }
}

uint64_t MythBDPlayer::GetBookmark(void)
{
    if (gCoreContext->IsDatabaseIgnored() || !m_playerCtx->m_buffer->IsBD())
        return 0;

    QString name;
    QString serialid;
    uint64_t frames = 0;

    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);

    if (m_playerCtx->m_playingInfo)
    {
        if (!m_playerCtx->m_buffer->BD()->GetNameAndSerialNum(name, serialid))
        {
            m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
            return 0;
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
    return frames;;
}

void MythBDPlayer::CreateDecoder(char *testbuf, int testreadsize)
{
    if (AvFormatDecoderBD::CanHandle(testbuf, m_playerCtx->m_buffer->GetFilename(),
                                     testreadsize))
    {
        SetDecoder(new AvFormatDecoderBD(this, *m_playerCtx->m_playingInfo,
                                         m_playerFlags));
    }
}
