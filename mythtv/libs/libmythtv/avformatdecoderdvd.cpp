#include "dvdringbuffer.h"
#include "avformatdecoderdvd.h"

AvFormatDecoderDVD::AvFormatDecoderDVD(
    MythPlayer *parent, const ProgramInfo &pginfo,
    bool use_null_video_out, bool allow_private_decode,
    bool no_hardware_decode, AVSpecialDecode av_special_decode)
  : AvFormatDecoder(parent, pginfo, use_null_video_out, allow_private_decode,
                    no_hardware_decode, av_special_decode)
{
}

int AvFormatDecoderDVD::GetAudioLanguage(uint audio_index, uint stream_index)
{
    (void)audio_index;
    if (ringBuffer && ringBuffer->IsDVD())
    {
        return ringBuffer->DVD()->GetAudioLanguage(
            ringBuffer->DVD()->GetAudioTrackNum(ic->streams[stream_index]->id));
    }
    return iso639_str3_to_key("und");
}
