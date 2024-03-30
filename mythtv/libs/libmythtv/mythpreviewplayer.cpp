// Qt
#include <QThread>

// MythTV
#include "mythpreviewplayer.h"

#define LOC QString("PreviewPlayer: ")

MythPreviewPlayer::MythPreviewPlayer(PlayerContext* Context, PlayerFlags Flags)
  : MythPlayer(Context, Flags)
{
}

/*! \brief Returns one RGB frame grab from a video
 *
 *   User is responsible for deleting the buffer with delete[].
 *   This also tries to skip any commercial breaks for a more
 *   useful screen grab for previews.
 *
 *   Warning: Don't use this on something you're playing!
 *
 *  \param SecondsIn   [in]  Seconds to seek into the buffer
 *  \param BufferSize  [out] Size of buffer returned in bytes
 *  \param FrameWidth  [out] Width of buffer returned
 *  \param FrameHeight [out] Height of buffer returned
 *  \param AspectRatio [out] Aspect of buffer returned
 */
char *MythPreviewPlayer::GetScreenGrab(std::chrono::seconds SecondsIn, int& BufferSize,
                                       int& FrameWidth, int& FrameHeight, float& AspectRatio)
{
    auto frameNum = static_cast<uint64_t>(SecondsIn.count() * m_videoFrameRate);
    return GetScreenGrabAtFrame(frameNum, false, BufferSize, FrameWidth, FrameHeight, AspectRatio);
}

/*! \brief Returns one RGB frame grab from a video
 *
 *   User is responsible for deleting the buffer with delete[].
 *   This also tries to skip any commercial breaks for a more
 *   useful screen grab for previews.
 *
 *   Warning: Don't use this on something you're playing!
 *
 *  \param FrameNum    [in]  Frame number to capture
 *  \param Absolute    [in]  If False, make sure we aren't in cutlist or Comm brk
 *  \param BufferSize  [out] Size of buffer returned in bytes
 *  \param FrameWidth  [out] Width of buffer returned
 *  \param FrameHeight [out] Height of buffer returned
 *  \param AspectRatio [out] Aspect of buffer returned
 */
char *MythPreviewPlayer::GetScreenGrabAtFrame(uint64_t FrameNum, bool Absolute, int& BufferSize,
                                              int& FrameWidth, int& FrameHeight, float& AspectRatio)
{
    BufferSize = 0;
    FrameWidth = FrameHeight = 0;
    AspectRatio = 0;

    if (OpenFile(0) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not open file for preview.");
        return nullptr;
    }

    bool fail = false;

    // No video to grab
    if ((m_videoDim.width() <= 0) || (m_videoDim.height() <= 0))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No video for preview in file '%1'")
            .arg(m_playerCtx->m_buffer->GetSafeFilename()));
        fail = true;
    }

    // We may have a BluRay or DVD buffer but this class does not inherit
    // from MythBDPlayer or MythDVDPlayer - so no seeking/grabbing
    if (m_playerCtx->m_buffer->IsBD())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Cannot generate preview for BluRay file '%1'")
            .arg(m_playerCtx->m_buffer->GetSafeFilename()));
        fail = true;
    }

    if (m_playerCtx->m_buffer->IsDVD())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Cannot generate preview for DVD file '%1'")
            .arg(m_playerCtx->m_buffer->GetSafeFilename()));
        fail = true;
    }

    if (fail)
    {
        FrameWidth = 640;
        FrameHeight = 480;
        AspectRatio = 4.0F / 3.0F;
        BufferSize = FrameWidth * FrameHeight * 4;
        char* result = new char[static_cast<size_t>(BufferSize)];
        memset(result, 0x3f, static_cast<size_t>(BufferSize) * sizeof(char));
        return result;
    }

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to initialize video for screen grab.");
        return nullptr;
    }

    ClearAfterSeek();
    if (!m_decoderThread)
        DecoderStart(true /*start paused*/);
    uint64_t dummy = 0;
    SeekForScreenGrab(dummy, FrameNum, Absolute);
    int tries = 0;
    while (!m_videoOutput->ValidVideoFrames() && (tries < 500))
    {
        tries += 1;
        m_decodeOneFrame = true;
        QThread::usleep(10000);
        if ((tries % 10) == 0)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Waited 100ms for video frame");
    }

    MythVideoFrame *frame = m_videoOutput->GetLastDecodedFrame();
    if (frame == nullptr)
        return nullptr;
    if (!frame->m_buffer)
        return nullptr;

    if (frame->m_interlaced)
    {
        // Use high quality - which is currently yadif
        frame->m_deinterlaceDouble = DEINT_NONE;
        frame->m_deinterlaceAllowed = frame->m_deinterlaceSingle = DEINT_CPU | DEINT_HIGH;
        MythDeinterlacer deinterlacer;
        deinterlacer.Filter(frame, kScan_Interlaced, nullptr, true);
    }
    uint8_t* result = MythVideoFrame::CreateBuffer(FMT_RGB32, m_videoDim.width(), m_videoDim.height());
    MythAVCopy copyCtx;
    AVFrame retbuf;
    memset(&retbuf, 0, sizeof(AVFrame));
    copyCtx.Copy(&retbuf, frame, result, AV_PIX_FMT_RGB32);
    FrameWidth = m_videoDispDim.width();
    FrameHeight = m_videoDispDim.height();
    AspectRatio = frame->m_aspect;

    DiscardVideoFrame(frame);
    return reinterpret_cast<char*>(result);
}

void MythPreviewPlayer::SeekForScreenGrab(uint64_t& Number, uint64_t FrameNum, bool Absolute)
{
    Number = FrameNum;
    if (Number >= m_totalFrames)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Screen grab requested for frame number beyond end of file.");
        Number = m_totalFrames / 2;
    }

    if (!Absolute && m_hasFullPositionMap)
    {
        m_bookmarkSeek = GetBookmark();
        // Use the bookmark if we should, otherwise make sure we aren't
        // in the cutlist or a commercial break
        if (m_bookmarkSeek > 30)
        {
            Number = m_bookmarkSeek;
        }
        else
        {
            uint64_t oldnumber = Number;
            m_deleteMap.LoadMap();
            m_commBreakMap.LoadMap(m_playerCtx, m_framesPlayed);

            bool started_in_break_map = false;
            while (m_commBreakMap.IsInCommBreak(Number) || IsInDelete(Number))
            {
                started_in_break_map = true;
                Number += static_cast<uint64_t>(30 * m_videoFrameRate);
                if (Number >= m_totalFrames)
                {
                    Number = oldnumber;
                    break;
                }
            }

            // Advance a few seconds from the end of the break
            if (started_in_break_map)
            {
                oldnumber = Number;
                Number += static_cast<uint64_t>(10 * m_videoFrameRate);
                if (Number >= m_totalFrames)
                    Number = oldnumber;
            }
        }
    }

    DiscardVideoFrame(m_videoOutput->GetLastDecodedFrame());
    DoJumpToFrame(Number, kInaccuracyNone);
}
