#ifndef MYTHVIDEODRMUTILS_H
#define MYTHVIDEODRMUTILS_H

// MythTV
#include "libmythui/platforms/drm/mythdrmproperty.h"

class MythVideoDRMUtils
{
  public:
    static uint64_t FFmpegColorRangeToDRM    (const DRMProp& Property, int Range);
    static uint64_t FFmpegColorEncodingToDRM (const DRMProp& Property, int Encoding);
};

#endif
