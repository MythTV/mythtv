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
    int       ReadPacket          (AVFormatContext *Ctx, AVPacket* Pkt, bool& StorePacket) override;

  protected:
    bool      IsValidStream       (int StreamId) override;

  private:
    bool      DoRewindSeek        (long long DesiredFrame) override;
    void      DoFastForwardSeek   (long long DesiredFrame, bool &Needflush) override;
    void      StreamChangeCheck   (void) override;
    int       GetSubtitleLanguage (uint SubtitleIndex, uint StreamIndex) override;
    int       GetAudioLanguage    (uint AudioIndex, uint StreamIndex) override;

    long long BDFindPosition      (long long DesiredFrame);
};

#endif
