/**
 * \file     mediamonitor-windows.cpp
 * \brief    MythMediaMonitor for Windows
 */

#undef UNICODE
#define _WIN32_WINNT 0x0500
#include "compat.h"

#include "mediamonitor-windows.h"
#include "mythcdrom.h"
#include "mythhdd.h"
#include "mythlogging.h"


/**
 * \class MediaMonitorWindows
 *
 * I am assuming, for now, that everything on Windows uses drive letters
 * (e.g. C:, D:). That is probably wrong, though. (other APIs?)
 */
MediaMonitorWindows::MediaMonitorWindows(QObject* par,
                                         unsigned long interval,
                                         bool allowEject)
                   : MediaMonitor(par, interval, allowEject)
{
    char strDrives[128];
    if (!::GetLogicalDriveStrings(sizeof(strDrives), strDrives))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Error. MediaMonitorWindows failed at GetLogicalDriveStrings.");
        return;
    }

    for (char *driveName = strDrives; *driveName;
         driveName += strlen(driveName) + 1)
    {
        MythMediaDevice *media = NULL;
        UINT type = ::GetDriveType(driveName);
        switch (type)
        {
        case DRIVE_CDROM:
            LOG(VB_MEDIA, LOG_DEBUG,
                QString("MediaMonitorWindows found cdrom '%1'").arg(driveName));
            media = MythCDROM::get(this, driveName, false, allowEject);
            break;
        case DRIVE_REMOVABLE:
            LOG(VB_MEDIA, LOG_DEBUG,
                QString("MediaMonitorWindows found removeable '%1'")
                    .arg(driveName));
            media = MythHDD::Get(this, driveName, false, allowEject);
            break;
        case DRIVE_UNKNOWN:
            LOG(VB_MEDIA, LOG_DEBUG,
                QString("MediaMonitorWindows found unknown '%1'")
                    .arg(driveName));
            media = MythCDROM::get(this, driveName, false, allowEject);
            break;
        case DRIVE_NO_ROOT_DIR:
            LOG(VB_MEDIA, LOG_DEBUG,
                QString("MediaMonitorWindows found '%1' with no root dir")
                    .arg(driveName));
            media = MythCDROM::get(this, driveName, false, allowEject);
            break;
        default:
            LOG(VB_MEDIA, LOG_INFO, QString("MediaMonitorWindows found '%1' type %2")
                .arg(driveName).arg(type));
        case DRIVE_FIXED:
        case DRIVE_REMOTE:
        case DRIVE_RAMDISK:
            continue;
        }

        if (media)
        {
            // We store the volume name to improve
            // user activities like ChooseAndEjectMedia().
            char volumeName[MAX_PATH];
            if (GetVolumeInformation(driveName, volumeName, MAX_PATH,
                                     NULL, NULL, NULL, NULL, NULL))
            {
                media->setVolumeID(volumeName);
            }

            AddDevice(media);
        }
        else
            LOG(VB_GENERAL, LOG_ALERT,
                    "Error. Couldn't create MythMediaDevice.");
    }

    LOG(VB_MEDIA, LOG_INFO, "Initial device list: " + listDevices());
}

bool MediaMonitorWindows::AddDevice(MythMediaDevice *pDevice)
{
    if (!pDevice)
    {
        LOG(VB_GENERAL, LOG_ERR, "MediaMonitorWindows::AddDevice(null)");
        return false;
    }

    QString path = pDevice->getDevicePath();

    //
    // Check if this is a duplicate of a device we have already added
    //
    QList<MythMediaDevice*>::const_iterator itr = m_Devices.begin();
    for (; itr != m_Devices.end(); ++itr)
    {
        if ((*itr)->getDevicePath() == path)
        {
            LOG(VB_MEDIA, LOG_INFO,
                     "MediamonitorWindows::AddDevice() -- " +
                     QString("Not adding '%1', it appears to be a duplicate.")
                         .arg(path));

            return false;
        }
    }

    // TODO - either look up the model, or leave blank
    pDevice->setDeviceModel(path.toLocal8Bit().constData());

    QMutexLocker locker(&m_DevicesLock);
    connect(pDevice, SIGNAL(statusChanged(MythMediaStatus, MythMediaDevice*)),
            this, SLOT(mediaStatusChanged(MythMediaStatus, MythMediaDevice*)));
    m_Devices.push_back(pDevice);
    m_UseCount[pDevice] = 0;

    return true;
}

QStringList MediaMonitorWindows::GetCDROMBlockDevices()
{
    QStringList  list;

    char strDrives[128];
    if (::GetLogicalDriveStrings(sizeof(strDrives), strDrives))
    {
        for (char* driveName = strDrives; *driveName;
             driveName += strlen(driveName) + 1)
        {
            if (::GetDriveType(driveName) == DRIVE_CDROM)
                list.append(driveName);
        }
    }

    return list;
}
