#include <QDir>
#include <QFileInfo>

#include "mythcdrom.h"
#include "mythconfig.h"
#ifdef linux
#include "mythcdrom-linux.h"
#elif defined(__FreeBSD__)
#include "mythcdrom-freebsd.h"
#elif CONFIG_DARWIN
#include "mythcdrom-darwin.h"
#endif
#include "mythlogging.h"


// If your DVD has directories in lowercase, then it is wrongly mounted!
// DVDs use the UDF filesystem, NOT ISO9660. Fix your /etc/fstab.

// This allows a warning for the above mentioned OS setup fault
#define PATHTO_BAD_DVD_MOUNT "/video_ts"

#define PATHTO_DVD_DETECT "/VIDEO_TS"
#define PATHTO_BD_DETECT "/BDMV"
#define PATHTO_VCD_DETECT "/vcd"
#define PATHTO_SVCD_DETECT "/svcd"

// Mac OS X mounts audio CDs ready to use
#define PATHTO_AUDIO_DETECT "/.TOC.plist"


MythCDROM* MythCDROM::get(QObject* par, const char* devicePath,
                          bool SuperMount, bool AllowEject)
{
#ifdef linux
    return GetMythCDROMLinux(par, devicePath, SuperMount, AllowEject);
#elif defined(__FreeBSD__)
    return GetMythCDROMFreeBSD(par, devicePath, SuperMount, AllowEject);
#elif CONFIG_DARWIN
    return GetMythCDROMDarwin(par, devicePath, SuperMount, AllowEject);
#else
    return new MythCDROM(par, devicePath, SuperMount, AllowEject);
#endif
}

MythCDROM::MythCDROM(QObject* par, const char* DevicePath, bool SuperMount, 
                     bool AllowEject) 
         : MythMediaDevice(par, DevicePath, SuperMount, AllowEject)
{
}

void MythCDROM::onDeviceMounted()
{
    if (!QDir(m_MountPath).exists())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Mountpoint '%1' doesn't exist")
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
    QDir         bd = QDir(m_MountPath  + PATHTO_BD_DETECT);

    // Default is data media
    m_MediaType = MEDIATYPE_DATA;

    // Default is mounted media
    m_Status = MEDIASTAT_MOUNTED;

    if (dvd.exists())
    {
        LOG(VB_MEDIA, LOG_INFO, "Probable DVD detected.");
        m_MediaType = MEDIATYPE_DVD;
        m_Status = MEDIASTAT_USEABLE; 
    }
    else if (bd.exists())
    {
        LOG(VB_MEDIA, LOG_INFO, "Probable Blu-ray detected.");
        m_MediaType = MEDIATYPE_BD;
        m_Status = MEDIASTAT_USEABLE;
    }
    else if (audio.exists())
    {
        LOG(VB_MEDIA, LOG_INFO, "Probable Audio CD detected.");
        m_MediaType = MEDIATYPE_AUDIO;
        m_Status = MEDIASTAT_USEABLE; 
    }
    else if (vcd.exists() || svcd.exists())
    {
        LOG(VB_MEDIA, LOG_INFO, "Probable VCD/SVCD detected.");
        m_MediaType = MEDIATYPE_VCD;
        m_Status = MEDIASTAT_USEABLE; 
    }
    else if (bad_dvd.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, 
            "DVD incorrectly mounted? (ISO9660 instead of UDF)");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
                QString("CD/DVD '%1' contained none of\n").arg(m_MountPath) +
                QString("\t\t\t%1, %2, %3 or %4").arg(PATHTO_DVD_DETECT)
                .arg(PATHTO_AUDIO_DETECT).arg(PATHTO_VCD_DETECT)
                .arg(PATHTO_SVCD_DETECT));
        LOG(VB_GENERAL, LOG_INFO, "Searching CD statistically - file by file!");
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

void MythCDROM::setDeviceSpeed(const char *devicePath, int speed)
{
    LOG(VB_MEDIA, LOG_INFO, 
        QString("SetDeviceSpeed(%1,%2) - not implemented on this OS.")
        .arg(devicePath).arg(speed));
}
