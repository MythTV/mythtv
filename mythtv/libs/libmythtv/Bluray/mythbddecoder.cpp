// MythTV
#include "libmythbase/iso639.h"
#include "Bluray/mythbdplayer.h"
#include "Bluray/mythbdbuffer.h"
#include "Bluray/mythbddecoder.h"

#define LOC QString("BDDecoder: ")

MythBDDecoder::MythBDDecoder(MythPlayer *Parent, const ProgramInfo &PGInfo, PlayerFlags Flags)
  : AvFormatDecoder(Parent, PGInfo, Flags)
{
}

bool MythBDDecoder::IsValidStream(int StreamId)
{
    if (m_ringBuffer && m_ringBuffer->IsBD())
        return m_ringBuffer->BD()->IsValidStream(static_cast<uint>(StreamId));
    return AvFormatDecoder::IsValidStream(StreamId);
}

void MythBDDecoder::Reset(bool ResetVideoData, bool SeekReset, bool ResetFile)
{
    AvFormatDecoder::Reset(ResetVideoData, SeekReset, ResetFile);
    SyncPositionMap();
}

void MythBDDecoder::UpdateFramesPlayed(void)
{
    if (!m_ringBuffer->IsBD())
        return;

    auto currentpos = static_cast<long long>(m_ringBuffer->BD()->GetCurrentTime().count() * m_fps);
    m_framesPlayed = m_framesRead = currentpos ;
    m_parent->SetFramesPlayed(static_cast<uint64_t>(currentpos + 1));
}

bool MythBDDecoder::DoRewindSeek(long long DesiredFrame)
{
    if (!m_ringBuffer->IsBD())
        return false;

    m_ringBuffer->Seek(BDFindPosition(DesiredFrame), SEEK_SET);
    m_framesPlayed = m_framesRead = m_lastKey = DesiredFrame + 1;
    m_frameCounter += 100;
    return true;
}

void MythBDDecoder::DoFastForwardSeek(long long DesiredFrame, bool &Needflush)
{
    if (!m_ringBuffer->IsBD())
        return;

    m_ringBuffer->Seek(BDFindPosition(DesiredFrame), SEEK_SET);
    Needflush      = true;
    m_framesPlayed = m_framesRead = m_lastKey = DesiredFrame + 1;
    m_frameCounter += 100;
}

void MythBDDecoder::StreamChangeCheck(void)
{
    if (!m_ringBuffer->IsBD())
        return;

    if (m_streamsChanged)
    {
        // This was originally in HandleBDStreamChange
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "resetting");
        QMutexLocker locker(&m_trackLock);
        Reset(true, false, false);
        CloseCodecs();
        FindStreamInfo();
        ScanStreams(false);
        m_streamsChanged=false;
    }

    if (m_parent->AtNormalSpeed() && m_ringBuffer->BD()->TitleChanged())
    {
        ResetPosMap();
        SyncPositionMap();
        UpdateFramesPlayed();
    }
}

int MythBDDecoder::GetSubtitleLanguage(uint /*SubtitleIndex*/, uint StreamIndex)
{
    if (m_ringBuffer && m_ringBuffer->IsBD() && (StreamIndex < m_ic->nb_streams) &&
        (m_ic->streams[StreamIndex] != nullptr))
    {
        return m_ringBuffer->BD()->GetSubtitleLanguage(static_cast<uint>(m_ic->streams[StreamIndex]->id));
    }

    return iso639_str3_to_key("und");
}

int MythBDDecoder::GetAudioLanguage(uint /*AudioIndex*/, uint StreamIndex)
{
    if (m_ringBuffer && m_ringBuffer->IsBD() && (StreamIndex < m_ic->nb_streams) &&
        (m_ic->streams[StreamIndex] != nullptr))
    {
        return m_ringBuffer->BD()->GetAudioLanguage(static_cast<uint>(m_ic->streams[StreamIndex]->id));
    }

    return iso639_str3_to_key("und");
}

int MythBDDecoder::ReadPacket(AVFormatContext *Ctx, AVPacket* Pkt, bool& /*StorePacket*/)
{
    m_avCodecLock.lock();
    int result = av_read_frame(Ctx, Pkt);
    m_avCodecLock.unlock();
    /* If we seem to have hit the end of the file, the ringbuffer may
     * just be blocked in order to drain the ffmpeg buffers, so try
     * unblocking it and reading again.
     * If ffmpeg's buffers are empty, it takes a couple of goes to
     * fill and read from them.
     */
    for (int count = 0; (count < 3) && (result == AVERROR_EOF); count++)
    {
        if (m_ringBuffer->BD()->IsReadingBlocked())
            m_ringBuffer->BD()->UnblockReading();
        m_avCodecLock.lock();
        result = av_read_frame(Ctx, Pkt);
        m_avCodecLock.unlock();
    }

    if (result >= 0)
    {
        Pkt->dts = m_ringBuffer->BD()->AdjustTimestamp(Pkt->dts);
        Pkt->pts = m_ringBuffer->BD()->AdjustTimestamp(Pkt->pts);
    }

    return result;
}

long long MythBDDecoder::BDFindPosition(long long DesiredFrame)
{
    if (!m_ringBuffer->IsBD())
        return 0;

    int ffrewSkip = 1;
    int current_speed = 0;
    if (m_parent)
    {
        ffrewSkip = m_parent->GetFFRewSkip();
        current_speed = static_cast<int>(m_parent->GetNextPlaySpeed());
    }

    if (ffrewSkip == 1 || ffrewSkip == 0)
    {
#if 0
        int diffTime = (int)ceil((desiredFrame - m_framesPlayed) / m_fps);
        long long desiredTimePos = m_ringBuffer->BD()->GetCurrentTime() +
                        diffTime;
        if (diffTime <= 0)
            desiredTimePos--;
        else
            desiredTimePos++;

        if (desiredTimePos < 0)
            desiredTimePos = 0;
#endif
        return static_cast<long long>(DesiredFrame * 90000.0 / m_fps);
    }
    return current_speed;
}
