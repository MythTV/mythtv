#ifndef CUTTER_H
#define CUTTER_H

#include <cstdint>                      // for int64_t
#include "libmythbase/programtypes.h"   // for frm_dir_map_t
#include "libmythtv/deletemap.h"        // for DeleteMap

// Cutter object is used in performing clean cutting. The
// act of cutting is shared between the player and the
// transcode loop. The player performs the initial part
// of the cut by seeking, and the transcode loop handles
// the remaining part by discarding data.
class Cutter
{
  public:
    Cutter() = default;
    void          SetCutList(frm_dir_map_t &deleteMap, PlayerContext *ctx);
    frm_dir_map_t AdjustedCutList() const;
    void          Activate(float v2a, int64_t total);
    void          NewFrame(int64_t currentFrame);
    bool          InhibitUseVideoFrame(void);
    bool          InhibitUseAudioFrames(int64_t frames, long *totalAudio);
    bool          InhibitDummyFrame(void);
    bool          InhibitDropFrame(void);

  private:
    bool          m_active {false};
    frm_dir_map_t m_foreshortenedCutList;
    DeleteMap     m_tracker;
    int64_t       m_totalFrames {0};
    int64_t       m_videoFramesToCut {0};
    int64_t       m_audioFramesToCut {0};
    float         m_audioFramesPerVideoFrame {0.0};
    static constexpr uint8_t kMaxLeadIn { 200 };
    static constexpr uint8_t kMinCut { 20 };
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */

