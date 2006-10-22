#include "mythcdrom.h"

#ifdef linux
#include "mythcdrom-linux.h"
#elif defined(__FreeBSD__)
#include "mythcdrom-freebsd.h"
#endif

#include "mythconfig.h"
#include "mythcontext.h"

#include <qdir.h>
#include <qfileinfo.h>

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

// Mac OS X mounts audio CDs ready to use
#ifndef PATHTO_AUDIO_DETECT
#define PATHTO_AUDIO_DETECT "/.TOC.plist"
#endif


MythCDROM* MythCDROM::get(QObject* par, const char* devicePath, bool SuperMount,
                                 bool AllowEject) {
#ifdef linux
    return GetMythCDROMLinux(par, devicePath, SuperMount, AllowEject);
#elif defined(__FreeBSD__)
    return GetMythCDROMFreeBSD(par, devicePath, SuperMount, AllowEject);
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
    if (!QDir(m_MountPath).exists())
    {
        VERBOSE(VB_IMPORTANT, QString("Mountpoint '%1' doesn't exist")
                              .arg(m_MountPath));
        m_MediaType = MEDIATYPE_UNKNOWN;
        m_Status    = MEDIASTAT_ERROR;
        return;
    }

    QFileInfo audio = QFileInfo(m_MountPath + PATHTO_AUDIO_DETECT);
    QDir        dvd = QDir(m_MountPath  + PATHTO_DVD_DETECT);
    QDir       svcd = QDir(m_MountPath  + PATHTO_SVCD_DETECT);
    QDir        vcd = QDir(m_MountPath  + PATHTO_VCD_DETECT);

    // Default is data media
    m_MediaType = MEDIATYPE_DATA;

    // Default is mounted media
    m_Status = MEDIASTAT_MOUNTED;

    if (dvd.exists())
    {
        VERBOSE(VB_GENERAL, "Probable DVD detected.");
        m_MediaType = MEDIATYPE_DVD;
        // HACK make it possible to eject a DVD by unmounting it
        performMountCmd(false);
        m_Status = MEDIASTAT_USEABLE; 
    }
    else if (audio.exists())
    {
        VERBOSE(VB_GENERAL, "Probable Audio CD detected.");
        m_MediaType = MEDIATYPE_AUDIO;
        m_Status = MEDIASTAT_USEABLE; 
    }
    else if (vcd.exists() || svcd.exists())
    {
        VERBOSE(VB_GENERAL, "Probable VCD/SVCD detected.");
        m_MediaType = MEDIATYPE_VCD;
        // HACK make it possible to eject a VCD/SVCD by unmounting it
        performMountCmd(false);
        m_Status = MEDIASTAT_USEABLE; 
    }

    // If not DVD/AudioCD/VCD/SVCD, use parent's more generic version
    if (MEDIATYPE_DATA == m_MediaType)
        MythMediaDevice::onDeviceMounted();

    if (m_AllowEject)
        unlock();
}

