#ifndef MYTHDRMENCODER_H
#define MYTHDRMENCODER_H

// MythTV
#include "libmythui/platforms/drm/mythdrmresources.h"

using DRMEnc  = std::shared_ptr<class MythDRMEncoder>;
using DRMEncs = std::vector<DRMEnc>;

class MUI_PUBLIC MythDRMEncoder
{
  public:
    static DRMEnc  Create(int FD, uint32_t Id);
    static DRMEnc  GetEncoder(const DRMEncs& Encoders, uint32_t Id);
    static DRMEncs GetEncoders(int FD);

    uint32_t m_id            { 0 };
    uint32_t m_type          { 0 };
    uint32_t m_crtcId        { 0 };
    uint32_t m_possibleCrtcs { 0 };

  protected:
    explicit MythDRMEncoder(int FD, uint32_t Id);

  private:
    Q_DISABLE_COPY(MythDRMEncoder)
};

#endif
