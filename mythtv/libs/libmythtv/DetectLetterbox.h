// -*- Mode: c++ -*-

#include "mythplayer.h"

using namespace std;

class MythPlayer;

class MPUBLIC DetectLetterbox
{
public:
    DetectLetterbox(MythPlayer* const player);
    ~DetectLetterbox();
    void SetDetectLetterbox(bool detect);
    bool GetDetectLetterbox();
    void Detect(VideoFrame *frame);
    void SwitchTo(VideoFrame *frame);

private:
    bool isDetectLetterbox;
    int firstFrameChecked;

    AdjustFillMode detectLetterboxDefaultMode;
    AdjustFillMode detectLetterboxDetectedMode; // Wich mode was last detected
    long long detectLetterboxSwitchFrame; // On wich frame was the mode switch detected
    long long detectLetterboxPossibleHalfFrame;
    long long detectLetterboxPossibleFullFrame;
    int detectLetterboxConsecutiveCounter;

    MythPlayer *m_player;

    int detectLetterboxLimit;
    QMutex detectLetterboxLock;
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
