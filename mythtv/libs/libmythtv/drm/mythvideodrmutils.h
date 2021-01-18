#ifndef MYTHVIDEODRMUTILS_H
#define MYTHVIDEODRMUTILS_H

// MythTV
#include "platforms/drm/mythdrmproperty.h"

class MythVideoDRMUtils
{
  public:
    static uint64_t FFmpegColorRangeToDRM    (DRMProp Property, int Range);
    static uint64_t FFmpegColorEncodingToDRM (DRMProp Property, int Encoding);
};

#endif
