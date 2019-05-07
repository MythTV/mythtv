#include "bdringbuffer.h"
#include "mythbdplayer.h"
#include "avformatdecoderbd.h"

#include "iso639.h"

#define LOC QString("AFD_BD: ")

AvFormatDecoderBD::AvFormatDecoderBD(
    MythPlayer *parent, const ProgramInfo &pginfo, PlayerFlags flags)
  : AvFormatDecoder(parent, pginfo, flags)
{
}

bool AvFormatDecoderBD::IsValidStream(int streamid)
{
    if (ringBuffer && ringBuffer->IsBD())
        return ringBuffer->BD()->IsValidStream(streamid);
    return AvFormatDecoder::IsValidStream(streamid);
}

void AvFormatDecoderBD::Reset(bool reset_video_data, bool seek_reset, bool reset_file)
{
    AvFormatDecoder::Reset(reset_video_data, seek_reset, reset_file);
    SyncPositionMap();
}

void AvFormatDecoderBD::UpdateFramesPlayed(void)
{
    if (!ringBuffer->IsBD())
        return;

    long long currentpos = (long long)(ringBuffer->BD()->GetCurrentTime() * m_fps);
    m_framesPlayed = m_framesRead = currentpos ;
    m_parent->SetFramesPlayed(currentpos + 1);
}

bool AvFormatDecoderBD::DoRewindSeek(long long desiredFrame)
{
    if (!ringBuffer->IsBD())
        return false;

    ringBuffer->Seek(BDFindPosition(desiredFrame), SEEK_SET);
    m_framesPlayed = m_framesRead = m_lastKey = desiredFrame + 1;
    return true;
}

void AvFormatDecoderBD::DoFastForwardSeek(long long desiredFrame, bool &needflush)
{
    if (!ringBuffer->IsBD())
        return;

    ringBuffer->Seek(BDFindPosition(desiredFrame), SEEK_SET);
    needflush    = true;
    m_framesPlayed = m_framesRead = m_lastKey = desiredFrame + 1;
}

void AvFormatDecoderBD::StreamChangeCheck(void)
{
    if (!ringBuffer->IsBD())
        return;

    if (m_streams_changed)
    {
        // This was originally in HandleBDStreamChange
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "resetting");
        QMutexLocker locker(avcodeclock);
        Reset(true, false, false);
        CloseCodecs();
        FindStreamInfo();
        ScanStreams(false);
        avcodeclock->unlock();
        m_streams_changed=false;
    }

    if (m_parent->AtNormalSpeed() && ringBuffer->BD()->TitleChanged())
    {
        ResetPosMap();
        SyncPositionMap();
        UpdateFramesPlayed();
    }
}

int AvFormatDecoderBD::GetSubtitleLanguage(uint subtitle_index,
                                           uint stream_index)
{
    (void)subtitle_index;
    if (ringBuffer && ringBuffer->IsBD() &&
        stream_index < m_ic->nb_streams &&
        m_ic->streams[stream_index] != nullptr)
    {
        return ringBuffer->BD()->GetSubtitleLanguage(m_ic->streams[stream_index]->id);
    }

    return iso639_str3_to_key("und");
}

int AvFormatDecoderBD::GetAudioLanguage(uint audio_index, uint stream_index)
{
    (void)audio_index;
    if (ringBuffer && ringBuffer->IsBD() &&
        stream_index < m_ic->nb_streams &&
        m_ic->streams[stream_index] != nullptr)
    {
        return ringBuffer->BD()->GetAudioLanguage(m_ic->streams[stream_index]->id);
    }

    return iso639_str3_to_key("und");
}

int AvFormatDecoderBD::ReadPacket(AVFormatContext *ctx, AVPacket* pkt, bool& /*storePacket*/)
{
    QMutexLocker locker(avcodeclock);

    int result = av_read_frame(ctx, pkt);

    /* If we seem to have hit the end of the file, the ringbuffer may
     * just be blocked in order to drain the ffmpeg buffers, so try
     * unblocking it and reading again.
     * If ffmpeg's buffers are empty, it takes a couple of goes to
     * fill and read from them.
     */
    for (int count = 0; (count < 3) && (result == AVERROR_EOF); count++)
    {
        if (ringBuffer->BD()->IsReadingBlocked())
            ringBuffer->BD()->UnblockReading();

        result = av_read_frame(ctx, pkt);
    }

    if (result >= 0)
    {
        pkt->dts = ringBuffer->BD()->AdjustTimestamp(pkt->dts);
        pkt->pts = ringBuffer->BD()->AdjustTimestamp(pkt->pts);
    }

    return result;
}

long long AvFormatDecoderBD::BDFindPosition(long long desiredFrame)
{
    if (!ringBuffer->IsBD())
        return 0;

    int ffrewSkip = 1;
    int current_speed = 0;
    if (m_parent)
    {
        ffrewSkip = m_parent->GetFFRewSkip();
        current_speed = (int)m_parent->GetNextPlaySpeed();
    }

    if (ffrewSkip == 1 || ffrewSkip == 0)
    {
#if 0
        int diffTime = (int)ceil((desiredFrame - m_framesPlayed) / m_fps);
        long long desiredTimePos = ringBuffer->BD()->GetCurrentTime() +
                        diffTime;
        if (diffTime <= 0)
            desiredTimePos--;
        else
            desiredTimePos++;

        if (desiredTimePos < 0)
            desiredTimePos = 0;
#endif
        return (desiredFrame * 90000LL / m_fps);
    }
    return current_speed;
}
