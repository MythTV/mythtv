// MythTV
#include "mythplayer.h"

class MTV_PUBLIC DetectLetterbox
{
  public:
    explicit DetectLetterbox(MythPlayer* const Player);
    ~DetectLetterbox() = default;
    void SetDetectLetterbox(bool Detect);
    bool GetDetectLetterbox() const;
    void Detect(VideoFrame *Frame);
    void SwitchTo(VideoFrame *Frame);

  private:
    bool           m_isDetectLetterbox                 { false };
    long long      m_firstFrameChecked                 { 0 };
    VideoFrameType m_frameType                         { FMT_NONE };
    AdjustFillMode m_detectLetterboxDefaultMode        { kAdjustFill_Off };
    AdjustFillMode m_detectLetterboxDetectedMode       { kAdjustFill_Off }; /// Which mode was last detected
    long long      m_detectLetterboxSwitchFrame        { -1 }; /// On which frame was the mode switch detected
    long long      m_detectLetterboxPossibleHalfFrame  { -1 };
    long long      m_detectLetterboxPossibleFullFrame  { -1 };
    int            m_detectLetterboxConsecutiveCounter { 0 };
    MythPlayer    *m_player                            { nullptr };
    int            m_detectLetterboxLimit              { 75 };
    QMutex         m_detectLetterboxLock;
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
