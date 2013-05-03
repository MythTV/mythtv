#ifndef CUTTER_H
#define CUTTER_H

#include "deletemap.h"
#include "programtypes.h"

// Cutter object is used in performing clean cutting. The
// act of cutting is shared between the player and the
// transcode loop. The player performs the initial part
// of the cut by seeking, and the transcode loop handles
// the remaining part by discarding data.
class Cutter
{
  public:
    Cutter() : active(false), videoFramesToCut(0), audioFramesToCut(0),
        audioFramesPerVideoFrame(0.0) {};

    void          SetCutList(frm_dir_map_t &deleteMap, PlayerContext *ctx);
    frm_dir_map_t AdjustedCutList() const;
    void          Activate(float v2a, int64_t total);
    void          NewFrame(int64_t currentFrame);
    bool          InhibitUseVideoFrame(void);
    bool          InhibitUseAudioFrames(int64_t frames, long *totalAudio);
    bool          InhibitDummyFrame(void);
    bool          InhibitDropFrame(void);

  private:
    bool          active;
    frm_dir_map_t foreshortenedCutList;
    DeleteMap     tracker;
    int64_t       totalFrames;
    int64_t       videoFramesToCut;
    int64_t       audioFramesToCut;
    float         audioFramesPerVideoFrame;
    enum
    {
        MAXLEADIN  = 200,
        MINCUT     = 20
    };

};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */

