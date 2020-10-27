#ifndef MYTHDETECTLETTERBOX_H
#define MYTHDETECTLETTERBOX_H

// Qt
#include <QMutex>

// MythTV
#include "mythframe.h"
#include "videoouttypes.h"

class MTV_PUBLIC DetectLetterbox
{
  public:
    DetectLetterbox();
   ~DetectLetterbox() = default;
    void SetDetectLetterbox(bool Detect, AdjustFillMode Mode);
    bool GetDetectLetterbox() const;
    bool Detect(MythVideoFrame* Frame, float VideoAspect, AdjustFillMode& Current);

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
    int            m_detectLetterboxLimit              { 75 };
};

#endif
