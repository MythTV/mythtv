#ifndef MYTHDRMFRAMEBUFFER_H
#define MYTHDRMFRAMEBUFFER_H

// MythTV
#include "libmythui/platforms/drm/mythdrmresources.h"

using DRMFb  = std::shared_ptr<class MythDRMFramebuffer>;
using DRMFbs = std::vector<DRMFb>;

class MythDRMFramebuffer
{
  public:
    static DRMFb Create(int FD, uint32_t Id);
    QString Description() const;

    uint32_t m_id        { 0 };
    uint32_t m_width     { 0 };
    uint32_t m_height    { 0 };
    uint32_t m_format    { 0 };
    uint64_t m_modifiers { 0 };
    DRMArray m_handles   { 0 };
    DRMArray m_pitches   { 0 };
    DRMArray m_offsets   { 0 };

  protected:
    MythDRMFramebuffer(int FD, uint32_t Id);

  private:
    Q_DISABLE_COPY(MythDRMFramebuffer)
};

#endif
