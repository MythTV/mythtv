#ifndef AVFORMATDECODERDVD_H
#define AVFORMATDECODERDVD_H

#include "avformatdecoder.h"

class AvFormatDecoderDVD : public AvFormatDecoder
{
  public:
    AvFormatDecoderDVD(MythPlayer *parent, const ProgramInfo &pginfo,
                       PlayerFlags flags);
    virtual void Reset(bool reset_video_data, bool seek_reset, bool reset_file);
    virtual void UpdateFramesPlayed(void);
    virtual bool GetFrame(DecodeType decodetype); // DecoderBase

  private:
    virtual bool DoRewindSeek(long long desiredFrame);
    virtual void DoFastForwardSeek(long long desiredFrame, bool &needflush);
    virtual void StreamChangeCheck(void);
    virtual void PostProcessTracks(void);
    virtual int GetAudioLanguage(uint audio_index, uint stream_index);
    virtual AudioTrackType GetAudioTrackType(uint stream_index);

    long long DVDFindPosition(long long desiredFrame);
};

#endif // AVFORMATDECODERDVD_H
