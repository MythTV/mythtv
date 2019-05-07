// -*- Mode: c++ -*-

#include "mythplayer.h"

using namespace std;

class MythPlayer;

class MTV_PUBLIC DetectLetterbox
{
public:
    explicit DetectLetterbox(MythPlayer* const player);
    ~DetectLetterbox() = default;
    void SetDetectLetterbox(bool detect);
    bool GetDetectLetterbox() const;
    void Detect(VideoFrame *frame);
    void SwitchTo(VideoFrame *frame);

private:
    bool           m_isDetectLetterbox;
    int            m_firstFrameChecked                 {0};

    AdjustFillMode m_detectLetterboxDefaultMode;
                   /// Which mode was last detected
    AdjustFillMode m_detectLetterboxDetectedMode;
                   /// On which frame was the mode switch detected
    long long      m_detectLetterboxSwitchFrame        {-1};
    long long      m_detectLetterboxPossibleHalfFrame  {-1};
    long long      m_detectLetterboxPossibleFullFrame  {-1};
    int            m_detectLetterboxConsecutiveCounter {0};

    MythPlayer    *m_player                            {nullptr};

    int            m_detectLetterboxLimit              {75};
    QMutex         m_detectLetterboxLock;
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
