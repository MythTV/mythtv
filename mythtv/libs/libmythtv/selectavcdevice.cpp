/**
 *  SelectAVCDevice
 *  Copyright (c) 2006 by Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "mythconfig.h"

#ifdef CONFIG_DARWIN
# include "mythcontext.h"
# include "selectavcdevice.h"
# undef always_inline
# include <AVCVideoServices/AVCVideoServices.h>

AVS::AVCDevice* SelectAVCDevice(
    AVS::AVCDeviceController* controller, 
    bool (*filter)(AVS::AVCDevice*)
)
{
    VERBOSE(VB_GENERAL, QString("SelectAVCDevice:"));

    for (unsigned n = CFArrayGetCount(controller->avcDeviceArray), 
             i = 0; i < n; ++i)
    {
        AVS::AVCDevice& d = *(AVS::AVCDevice*)CFArrayGetValueAtIndex(controller->avcDeviceArray, i);

        VERBOSE(
            VB_GENERAL, 
            QString("SelectAVCDevice: %1, format: %2, attached: %3, type: %4")
                .arg(d.deviceName)
                .arg(d.isDVDevice ? "DV" : d.isMPEGDevice ? "MPEG2-TS" : "unknown")
                .arg(d.isAttached ? "yes" : "no")
                .arg(d.hasTapeSubunit ? "tape" : d.hasMonitorOrTunerSubunit ? "tuner" : "unknown")
        );

        if (filter(&d))
        {
            VERBOSE(VB_GENERAL, QString("SelectAVCDevice: FOUND"));
            return &d;
        }
    }
    VERBOSE(VB_GENERAL, QString("SelectAVCDevice: NOT FOUND"));
    return 0;
}

#endif // CONFIG_DARWIN

