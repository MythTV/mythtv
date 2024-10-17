#ifndef MYTHTRANSCODEPLAYER_H
#define MYTHTRANSCODEPLAYER_H

// MythTV
#include "libmythtv/mythplayer.h"

class MythTranscodePlayer : public MythPlayer
{
  public:
    explicit MythTranscodePlayer(PlayerContext* Context, PlayerFlags Flags = kNoFlags);

    void SetTranscoding        (bool Transcoding);
    void InitForTranscode      ();
    void SetCutList            (const frm_dir_map_t& CutList);
    bool TranscodeGetNextFrame (int& DidFF, bool& KeyFrame, bool HonorCutList);
};

#endif
