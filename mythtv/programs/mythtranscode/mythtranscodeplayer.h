#ifndef MYTHTRANSCODEPLAYER_H
#define MYTHTRANSCODEPLAYER_H

// MythTV
#include "mythplayer.h"

class MythTranscodePlayer : public MythPlayer
{
  public:
    explicit MythTranscodePlayer(PlayerContext* Context, PlayerFlags Flags = kNoFlags);

    void SetTranscoding        (bool Transcoding);
    void InitForTranscode      (bool CopyAudio, bool CopyVideo);
    void SetCutList            (const frm_dir_map_t& CutList);
    bool TranscodeGetNextFrame (int& DidFF, bool& KeyFrame, bool HonorCutList);
    bool WriteStoredData       (MythMediaBuffer* OutBuffer, bool WriteVideo, std::chrono::milliseconds TimecodeOffset);
    long UpdateStoredFrameNum  (long CurrentFrameNum);
};

#endif
