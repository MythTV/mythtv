// MythTV
#include "mythtranscodeplayer.h"

#define LOC QString("TranscodePlayer: ")

MythTranscodePlayer::MythTranscodePlayer(PlayerContext *Context, PlayerFlags Flags)
  : MythPlayer(Context, Flags)
{
}

void MythTranscodePlayer::SetTranscoding(bool Transcoding)
{
    // TODO this is called too early to work but the second call to DecoderBase::SetTranscoding
    // is embedded in the middle of MythPlayer::OpenFile
    m_transcoding = Transcoding;
    if (m_decoder)
        m_decoder->SetTranscoding(Transcoding);
    else
        LOG(VB_GENERAL, LOG_WARNING, LOC + "No decoder yet - cannot set transcoding");
}

void MythTranscodePlayer::InitForTranscode(bool CopyAudio, bool CopyVideo)
{
    // Are these really needed?
    SetPlaying(true);
    m_keyframeDist = 30;

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to initialize video for transcode.");
        SetPlaying(false);
        return;
    }

    m_framesPlayed = 0;
    ClearAfterSeek();

    m_rawAudio = CopyAudio;
    m_rawVideo = CopyVideo;
    if (m_decoder)
        m_decoder->SetSeekSnap(0);
}

void MythTranscodePlayer::SetCutList(const frm_dir_map_t& CutList)
{
    m_deleteMap.SetMap(CutList);
}

bool MythTranscodePlayer::TranscodeGetNextFrame(int &DidFF, bool &KeyFrame, bool HonorCutList)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->UpdateInUseMark();
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    int64_t lastDecodedFrameNumber = m_videoOutput->GetLastDecodedFrame()->m_frameNumber;

    if ((lastDecodedFrameNumber == 0) && HonorCutList)
        m_deleteMap.TrackerReset(0);

    if (!m_decoderThread)
        DecoderStart(true/*start paused*/);

    if (!m_decoder)
        return false;

    {
        QMutexLocker decoderlocker(&m_decoderChangeLock);
        if (!DoGetFrame(kDecodeAV))
            return false;
    }

    if (GetEof() != kEofStateNone)
        return false;

    if (HonorCutList && !m_deleteMap.IsEmpty())
    {
        if (m_totalFrames && lastDecodedFrameNumber >= static_cast<int64_t>(m_totalFrames))
            return false;

        uint64_t jumpto = 0;
        if (m_deleteMap.TrackerWantsToJump(static_cast<uint64_t>(lastDecodedFrameNumber), jumpto))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Fast-Forwarding from %1 to %2")
                .arg(lastDecodedFrameNumber).arg(jumpto));
            if (jumpto >= m_totalFrames)
            {
                SetEof(kEofStateDelayed);
                return false;
            }

            // For 0.25, move this to DoJumpToFrame(jumpto)
            WaitForSeek(jumpto, 0);
            ClearAfterSeek();
            m_decoderChangeLock.lock();
            DoGetFrame(kDecodeAV);
            m_decoderChangeLock.unlock();
            DidFF = 1;
        }
    }
    if (GetEof() != kEofStateNone)
        return false;
    KeyFrame = m_decoder->IsLastFrameKey();
    return true;
}
