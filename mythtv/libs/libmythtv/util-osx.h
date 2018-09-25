//
//  util-osx.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 2/08/2015.
//  Copyright (c) 2015 Jean-Yves Avenard. All rights reserved.
//

#ifndef MythTV_util_osx_h
#define MythTV_util_osx_h

#include <cstdint>

extern "C" {
void GetOSXVersion(int32_t* aMajorVersion, int32_t* aMinorVersion);
}
#endif
