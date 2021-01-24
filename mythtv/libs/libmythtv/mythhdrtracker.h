#ifndef MYTHHDRTRACKER_H
#define MYTHHDRTRACKER_H

// MythTV
#include "mythhdrmetadata.h"

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
    explicit MythHDRTracker(int HDRSupport);
    virtual ~MythHDRTracker() = default;

    MythHDRPtr m_ffmpegMetadata { nullptr };
    int        m_hdrSupport     { 0 };
};

#endif
