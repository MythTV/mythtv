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


// If your DVD has directories in lowercase, then it is wrongly mounted!
// DVDs use the UDF filesystem, NOT ISO9660. Fix your /etc/fstab.

// This allows a warning for the above mentioned OS setup fault
#define PATHTO_BAD_DVD_MOUNT "/video_ts"

#define PATHTO_DVD_DETECT "/VIDEO_TS"

#define PATHTO_VCD_DETECT "/vcd"
#define PATHTO_SVCD_DETECT "/svcd"

// Mac OS X mounts audio CDs ready to use
#define PATHTO_AUDIO_DETECT "/.TOC.plist"


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
    QDir    bad_dvd = QDir(m_MountPath  + PATHTO_BAD_DVD_MOUNT);

    // Default is data media
    m_MediaType = MEDIATYPE_DATA;

    // Default is mounted media
    m_Status = MEDIASTAT_MOUNTED;

    if (dvd.exists())
    {
        VERBOSE(VB_MEDIA, "Probable DVD detected.");
        m_MediaType = MEDIATYPE_DVD;
        m_Status = MEDIASTAT_USEABLE; 
    }
    else if (audio.exists())
    {
        VERBOSE(VB_MEDIA, "Probable Audio CD detected.");
        m_MediaType = MEDIATYPE_AUDIO;
        m_Status = MEDIASTAT_USEABLE; 
    }
    else if (vcd.exists() || svcd.exists())
    {
        VERBOSE(VB_MEDIA, "Probable VCD/SVCD detected.");
        m_MediaType = MEDIATYPE_VCD;
        m_Status = MEDIASTAT_USEABLE; 
    }
    else if (bad_dvd.exists())
        VERBOSE(VB_IMPORTANT,
                "DVD incorrectly mounted? (ISO9660 instead of UDF)");
    else
    {
        VERBOSE(VB_GENERAL,
                QString("CD/DVD '%1' contained none of\n").arg(m_MountPath) +
                QString("\t\t\t%1, %2, %3 or %4").arg(PATHTO_DVD_DETECT)
                .arg(PATHTO_AUDIO_DETECT).arg(PATHTO_VCD_DETECT)
                .arg(PATHTO_SVCD_DETECT));
        VERBOSE(VB_GENERAL, "Searching CD statistically - file by file!");
    }

    // If not DVD/AudioCD/VCD/SVCD, use parent's more generic version
    if (MEDIATYPE_DATA == m_MediaType)
        MythMediaDevice::onDeviceMounted();


    // Unlock the door, and if appropriate unmount the media,
    // so the user can press the manual eject button
    if (m_AllowEject)
    {
        unlock();
        if (m_MediaType == MEDIATYPE_DVD || m_MediaType == MEDIATYPE_VCD)
            unmount();
    }
}

