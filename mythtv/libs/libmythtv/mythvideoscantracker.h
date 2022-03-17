#ifndef MYTHVIDEOSCANTRACKER_H
#define MYTHVIDEOSCANTRACKER_H

// MythTV
#include "libmythbase/mthread.h"
#include "mythframe.h"
#include "videoouttypes.h"

class MythPlayerUI;
class MythVideoOutput;

class MTV_PUBLIC MythVideoScanTracker
{
  public:
    explicit MythVideoScanTracker(MythPlayerUI* Parent);
    virtual ~MythVideoScanTracker() = default;

    FrameScanType  NextScanOverride     ();
    void           SetScanOverride      (FrameScanType Scan);
    FrameScanType  GetScanTypeWithOverride () const;

  protected:
    void           InitialiseScan       (MythVideoOutput* VideoOutput);
    void           UnlockScan           ();
    void           ResetTracker         ();

    void           SetScanType          (FrameScanType Scan, MythVideoOutput* VideoOutput,
                                         std::chrono::microseconds FrameInterval);
    FrameScanType  GetScanForDisplay    (MythVideoFrame* Frame, bool& SecondField);
    FrameScanType  GetScanType          () const;
    virtual void   AutoDeint            (MythVideoFrame* Frame, MythVideoOutput* VideoOutput,
                                         std::chrono::microseconds FrameInterval, bool AllowLock = true);
    void           CheckScanUpdate      (MythVideoOutput* VideoOutput,
                                         std::chrono::microseconds FrameInterval);
    QString        GetDeinterlacerName  ();
    FrameScanType  DetectInterlace      (FrameScanType NewScan, float Rate, int VideoHeight);

  private:
    MythPlayerUI*  m_parentPlayer       { nullptr };
    long long      m_scanTracker        { 0 };
    FrameScanType  m_resetScan          { kScan_Ignore     };
    FrameScanType  m_scan               { kScan_Interlaced };
    FrameScanType  m_scanOverride       { kScan_Detect     };
    bool           m_scanLocked         { false };
    bool           m_scanInitialized    { false };
    std::chrono::microseconds m_lastFrameInterval  { 0us   };
    bool           m_lastDeinterlacer2x { false };
    MythDeintType  m_lastDeinterlacer   { DEINT_NONE };
    VideoFrameType m_lastFrameCodec     { FMT_NONE };
    // Remove this
    QThread*       m_mainThread         { QThread::currentThread() };
};

#endif
