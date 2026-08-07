#ifndef AVFORMATDECODERBD_H
#define AVFORMATDECODERBD_H

// MythTV
#include "libmythtv/decoders/avformatdecoder.h"

class MythBDDecoder : public AvFormatDecoder
{
  public:
    MythBDDecoder(MythPlayer *Parent, const ProgramInfo &PGInfo, PlayerFlags Flags);

    void      Reset               (bool ResetVideoData, bool SeekReset, bool ResetFile) override;
    void      UpdateFramesPlayed  (void) override;
    int       GetSubtitleLanguage (uint SubtitleIndex, uint StreamIndex) override;
    int       GetAudioLanguage    (uint AudioIndex, uint StreamIndex) override;

  protected:
    int       ReadPacket          (AVFormatContext *Ctx, AVPacket* Pkt, bool& StorePacket) override;
    bool      IsValidStream       (int StreamId) override;
    bool      DoRewindSeek        (long long DesiredFrame) override;
    void      DoFastForwardSeek   (long long DesiredFrame, bool &Needflush) override;
    void      StreamChangeCheck   (void) override;

  private:
    long long BDFindPosition      (long long DesiredFrame);
};

#endif
