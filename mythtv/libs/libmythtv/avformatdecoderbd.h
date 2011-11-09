#ifndef AVFORMATDECODERBD_H
#define AVFORMATDECODERBD_H

#include "avformatdecoder.h"

class AvFormatDecoderBD : public AvFormatDecoder
{
  public:
    AvFormatDecoderBD(MythPlayer *parent, const ProgramInfo &pginfo,
                    bool use_null_video_out,
                    bool allow_private_decode = true,
                    bool no_hardware_decode = false,
                    AVSpecialDecode av_special_decode = kAVSpecialDecode_None);
    virtual void Reset(bool reset_video_data, bool seek_reset, bool reset_file);
    virtual void UpdateFramesPlayed(void);

  private:
    virtual bool DoRewindSeek(long long desiredFrame);
    virtual void DoFastForwardSeek(long long desiredFrame, bool &needflush);
    virtual void StreamChangeCheck(void);
    virtual int GetSubtitleLanguage(uint subtitle_index, uint stream_index);
    virtual int GetAudioLanguage(uint audio_index, uint stream_index);

    long long BDFindPosition(long long desiredFrame);
};

#endif // AVFORMATDECODERBD_H
