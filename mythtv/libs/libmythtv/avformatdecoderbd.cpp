#include "bdringbuffer.h"
#include "mythbdplayer.h"
#include "avformatdecoderbd.h"

#define LOC QString("AFD_BD: ")
#define LOC_ERR QString("AFD_BD Error: ")
#define LOC_WARN QString("AFD_BD Warning: ")

AvFormatDecoderBD::AvFormatDecoderBD(
    MythPlayer *parent, const ProgramInfo &pginfo,
    bool use_null_video_out, bool allow_private_decode,
    bool no_hardware_decode, AVSpecialDecode av_special_decode)
  : AvFormatDecoder(parent, pginfo, use_null_video_out, allow_private_decode,
                    no_hardware_decode, av_special_decode)
{
}

void AvFormatDecoderBD::Reset()
{
    AvFormatDecoder::Reset();
    SyncPositionMap();
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
