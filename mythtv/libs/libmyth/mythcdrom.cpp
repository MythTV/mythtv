#include "mythcdrom.h"
#include <sys/stat.h>

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

MythCDROM* MythCDROM::get(QObject* par, const char* devicePath, bool SuperMount,
                                 bool AllowEject) {
#ifdef linux
    return new MythCDROMLinux(par, devicePath, SuperMount, AllowEject);
#elif defined(__FreeBSD__)
    return new MythCDROMFreeBSD(par, devicePath, SuperMount, AllowEject);
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
    QString DetectPath;
    DetectPath.sprintf("%s%s", (const char*)m_MountPath, PATHTO_DVD_DETECT);
    VERBOSE(VB_ALL, QString("Looking for: '%1'").arg(DetectPath));

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
    VERBOSE(VB_ALL, QString("Looking for: '%1'").arg(DetectPath));

    if (stat(DetectPath, &sbuf) == 0)
    {
        VERBOSE(VB_GENERAL, "Probable VCD detected.");
        m_MediaType = MEDIATYPE_VCD;
        // HACK make it possible to eject a VCD by unmounting it
        performMountCmd(false);
        m_Status = MEDIASTAT_USEABLE; 
    }

    if (m_AllowEject)
        unlock();
}

