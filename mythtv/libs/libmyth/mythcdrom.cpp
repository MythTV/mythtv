#include "mythcdrom.h"
#include <sys/stat.h>

#include "mythconfig.h"
#include "mythcontext.h"

// For testing
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cerrno>

using namespace std;

// end for testing

#ifndef PATHTO_DVD_DETECT
#define PATHTO_DVD_DETECT "/VIDEO_TS"
#endif

#ifndef PATHTO_VCD_DETECT
#define PATHTO_VCD_DETECT "/vcd"
#endif

#ifndef PATHTO_SVCD_DETECT
#define PATHTO_SVCD_DETECT "/svcd"
#endif

MythCDROM* MythCDROM::get(QObject* par, const char* devicePath, bool SuperMount,
                                 bool AllowEject) {
#ifdef linux
    return new MythCDROMLinux(par, devicePath, SuperMount, AllowEject);
#elif defined(__FreeBSD__)
    return new MythCDROMFreeBSD(par, devicePath, SuperMount, AllowEject);
#elif defined(CONFIG_DARWIN)
    return new MythCDROM(par, devicePath, SuperMount, AllowEject);
#else
    return NULL;
#endif
}

MythCDROM::MythCDROM(QObject* par, const char* DevicePath, bool SuperMount, 
                     bool AllowEject) 
         : MythMediaDevice(par, DevicePath, SuperMount, AllowEject)
{
}

bool MythCDROM::openDevice()
{
    if (MythMediaDevice::openDevice()) 
    {
        // If allow eject is on, unlock the door.
        if (m_AllowEject)
            unlock();
        
        return true;
    }

    return false;
}

void MythCDROM::onDeviceMounted()
{
    QString DetectPath, DetectPath2;

    // Default is data media
    m_MediaType = MEDIATYPE_DATA;

    // Default is mounted media
    m_Status = MEDIASTAT_MOUNTED;

    DetectPath.sprintf("%s%s", (const char*)m_MountPath, PATHTO_DVD_DETECT);
    VERBOSE(VB_IMPORTANT, QString("Looking for: '%1'").arg(DetectPath));

    struct stat sbuf;
    if (stat(DetectPath, &sbuf) == 0)
    {
        VERBOSE(VB_GENERAL, "Probable DVD detected.");
        m_MediaType = MEDIATYPE_DVD;
        // HACK make it possible to eject a DVD by unmounting it
        performMountCmd(false);
        m_Status = MEDIASTAT_USEABLE; 
    }
    
    DetectPath.sprintf("%s%s", (const char*)m_MountPath, PATHTO_VCD_DETECT);
    VERBOSE(VB_IMPORTANT, QString("Looking for: '%1'").arg(DetectPath));

    DetectPath2.sprintf("%s%s", (const char*)m_MountPath, PATHTO_SVCD_DETECT);
    VERBOSE(VB_IMPORTANT, QString("Looking for: '%1'").arg(DetectPath2));

    if (stat(DetectPath, &sbuf) == 0 || stat(DetectPath2, &sbuf) == 0)
    {
        VERBOSE(VB_GENERAL, "Probable VCD/SVCD detected.");
        m_MediaType = MEDIATYPE_VCD;
        // HACK make it possible to eject a VCD/SVCD by unmounting it
        performMountCmd(false);
        m_Status = MEDIASTAT_USEABLE; 
    }

    // If not DVB or VCD, use parent to check file ext to determine media type
    if (MEDIATYPE_DATA == m_MediaType)
        MythMediaDevice::onDeviceMounted();

    if (m_AllowEject)
        unlock();
}

