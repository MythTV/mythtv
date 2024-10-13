#ifndef MYTHTRANSCODEPLAYER_H
#define MYTHTRANSCODEPLAYER_H

// MythTV
#include "libmythtv/mythplayer.h"

class MythTranscodePlayer : public MythPlayer
{
  public:
    explicit MythTranscodePlayer(PlayerContext* Context, PlayerFlags Flags = kNoFlags);

    void SetTranscoding        (bool Transcoding);
    void InitForTranscode      (bool CopyAudio, bool CopyVideo);
    void SetCutList            (const frm_dir_map_t& CutList);
    bool TranscodeGetNextFrame (int& DidFF, bool& KeyFrame, bool HonorCutList);

    void SetRawAudioState(bool state) { m_rawAudio = state; }
    bool GetRawAudioState() const     { return m_rawAudio ; }

    void SetRawVideoState(bool state) { m_rawVideo = state; }
    bool GetRawVideoState() const     { return m_rawVideo; }

  private:
    bool m_rawAudio {false};
    bool m_rawVideo {false};
};

#endif
