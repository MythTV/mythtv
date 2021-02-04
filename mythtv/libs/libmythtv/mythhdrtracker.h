#ifndef MYTHHDRTRACKER_H
#define MYTHHDRTRACKER_H

// MythTV
#include "mythhdrvideometadata.h"

// Std
#include <memory>

using HDRTracker = std::shared_ptr<class MythHDRTracker>;

class MythHDRTracker
{
  public:
    static HDRTracker Create(class MythDisplay* _Display);
    virtual void Update(class MythVideoFrame* Frame) = 0;
    virtual void Reset() = 0;

  protected:
    explicit MythHDRTracker(MythHDRPtr HDR);
    virtual ~MythHDRTracker() = default;

    MythHDRVideoPtr m_ffmpegMetadata { nullptr };
    MythHDRPtr      m_hdrSupport     { nullptr };
};

#endif
