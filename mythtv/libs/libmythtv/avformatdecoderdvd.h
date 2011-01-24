#ifndef AVFORMATDECODERDVD_H
#define AVFORMATDECODERDVD_H

#include "avformatdecoder.h"

class AvFormatDecoderDVD : public AvFormatDecoder
{
public:
    AvFormatDecoderDVD(MythPlayer *parent, const ProgramInfo &pginfo,
                    bool use_null_video_out,
                    bool allow_private_decode = true,
                    bool no_hardware_decode = false,
                    AVSpecialDecode av_special_decode = kAVSpecialDecode_None);

  private:
    virtual int GetAudioLanguage(uint audio_index, uint stream_index);
};

#endif // AVFORMATDECODERDVD_H
