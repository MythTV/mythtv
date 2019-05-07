#ifndef AVFORMATDECODERBD_H
#define AVFORMATDECODERBD_H

#include "avformatdecoder.h"

class AvFormatDecoderBD : public AvFormatDecoder
{
  public:
    AvFormatDecoderBD(MythPlayer *parent, const ProgramInfo &pginfo,
                      PlayerFlags flags);
    void Reset(bool reset_video_data, bool seek_reset, bool reset_file) override; // AvFormatDecoder
    void UpdateFramesPlayed(void) override; // AvFormatDecoder
    int ReadPacket(AVFormatContext *ctx, AVPacket* pkt, bool& storePacket) override; // AvFormatDecoder

  protected:
    bool IsValidStream(int streamid) override; // AvFormatDecoder

  private:
    bool DoRewindSeek(long long desiredFrame) override; // AvFormatDecoder
    void DoFastForwardSeek(long long desiredFrame, bool &needflush) override; // AvFormatDecoder
    void StreamChangeCheck(void) override; // AvFormatDecoder
    int GetSubtitleLanguage(uint subtitle_index, uint stream_index) override; // AvFormatDecoder
    int GetAudioLanguage(uint audio_index, uint stream_index) override; // AvFormatDecoder

    long long BDFindPosition(long long desiredFrame);
};

#endif // AVFORMATDECODERBD_H
