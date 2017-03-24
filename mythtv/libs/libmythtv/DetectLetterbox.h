// -*- Mode: c++ -*-

#include "mythplayer.h"

using namespace std;

class MythPlayer;

class MTV_PUBLIC DetectLetterbox
{
public:
    explicit DetectLetterbox(MythPlayer* const player);
    ~DetectLetterbox();
    void SetDetectLetterbox(bool detect);
    bool GetDetectLetterbox() const;
    void Detect(VideoFrame *frame);
    void SwitchTo(VideoFrame *frame);

private:
    bool isDetectLetterbox;
    int firstFrameChecked;

    AdjustFillMode detectLetterboxDefaultMode;
    AdjustFillMode detectLetterboxDetectedMode; ///< Which mode was last detected
    long long detectLetterboxSwitchFrame; ///< On which frame was the mode switch detected
    long long detectLetterboxPossibleHalfFrame;
    long long detectLetterboxPossibleFullFrame;
    int detectLetterboxConsecutiveCounter;

    MythPlayer *m_player;

    int detectLetterboxLimit;
    QMutex detectLetterboxLock;
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
