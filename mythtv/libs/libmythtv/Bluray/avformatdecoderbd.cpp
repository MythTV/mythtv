#include "bdringbuffer.h"
#include "mythbdplayer.h"
#include "avformatdecoderbd.h"

#define LOC QString("AFD_BD: ")

AvFormatDecoderBD::AvFormatDecoderBD(
    MythPlayer *parent, const ProgramInfo &pginfo, PlayerFlags flags)
  : AvFormatDecoder(parent, pginfo, flags)
{
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

    long long currentpos = (long long)(ringBuffer->BD()->GetCurrentTime() * fps);
    framesPlayed = framesRead = currentpos ;
    m_parent->SetFramesPlayed(currentpos + 1);
}

bool AvFormatDecoderBD::DoRewindSeek(long long desiredFrame)
{
    if (!ringBuffer->IsBD())
        return false;

    ringBuffer->Seek(BDFindPosition(desiredFrame), SEEK_SET);
    framesPlayed = framesRead = lastKey = desiredFrame + 1;
    return true;
}

void AvFormatDecoderBD::DoFastForwardSeek(long long desiredFrame, bool &needflush)
{
    if (!ringBuffer->IsBD())
        return;

    ringBuffer->Seek(BDFindPosition(desiredFrame), SEEK_SET);
    needflush    = true;
    framesPlayed = framesRead = lastKey = desiredFrame + 1;
}

void AvFormatDecoderBD::StreamChangeCheck(void)
{
    if (!ringBuffer->IsBD())
        return;

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
    (void)stream_index;
    if (ringBuffer && ringBuffer->IsBD())
        return ringBuffer->BD()->GetSubtitleLanguage(subtitle_index);
    return iso639_str3_to_key("und");
}

int AvFormatDecoderBD::GetAudioLanguage(uint audio_index, uint stream_index)
{
    (void)stream_index;
    if (ringBuffer && ringBuffer->IsBD())
        return ringBuffer->BD()->GetAudioLanguage(audio_index);
    return iso639_str3_to_key("und");
}

long long AvFormatDecoderBD::BDFindPosition(long long desiredFrame)
{
    if (!ringBuffer->IsBD())
        return 0;

    int diffTime = 0;
    long long desiredTimePos;
    int ffrewSkip = 1;
    int current_speed = 0;
    if (m_parent)
    {
        ffrewSkip = m_parent->GetFFRewSkip();
        current_speed = (int)m_parent->GetNextPlaySpeed();
    }

    if (ffrewSkip == 1 || ffrewSkip == 0)
    {
        diffTime = (int)ceil((desiredFrame - framesPlayed) / fps);
        desiredTimePos = ringBuffer->BD()->GetCurrentTime() +
                        diffTime;
        if (diffTime <= 0)
            desiredTimePos--;
        else
            desiredTimePos++;

        if (desiredTimePos < 0)
            desiredTimePos = 0;
        return (desiredFrame * 90000LL / fps);
    }
    return current_speed;
}
