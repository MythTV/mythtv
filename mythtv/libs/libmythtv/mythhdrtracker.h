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
    static HDRTracker Create(class MythDisplay* MDisplay);
    ~MythHDRTracker();
    void     Update(class MythVideoFrame* Frame);

  protected:
    explicit MythHDRTracker(MythHDRPtr HDR);
    void     Reset();

    MythHDRVideoPtr m_metadata { nullptr };
    MythHDRPtr      m_hdr      { nullptr };
};

#endif
