/**
 *  SelectAVCDevice
 *  Copyright (c) 2006 by Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef LIBMYTHTV_SELECTAVCDEVICE_H_
# define LIBMYTHTV_SELECTAVCDEVICE_H_

# include "mythconfig.h"

# ifdef CONFIG_DARWIN
#  undef always_inline
#  include <AVCVideoServices/AVCVideoServices.h>

AVS::AVCDevice* SelectAVCDevice(
    AVS::AVCDeviceController*, 
    bool (*)(AVS::AVCDevice*)
);

# endif // CONFIG_DARWIN

#endif LIBMYTHTV_SELECTAVCDEVICE_H_ 

