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
#include "mythverbose.h"


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
        return;

    for (char *driveName = strDrives; *driveName;
         driveName += strlen(driveName) + 1)
    {
        uint type = ::GetDriveType(driveName);
        if (type != DRIVE_REMOVABLE && type != DRIVE_CDROM)
            continue;

        MythMediaDevice *media = NULL;

        if (type == DRIVE_CDROM)
            media = MythCDROM::get(this, driveName, false, allowEject);
        else
            media = MythHDD::Get(this, driveName, false, allowEject);

        if (!media)
        {
            VERBOSE(VB_IMPORTANT,
                    "Error. Couldn't create MythMediaDevice.");
            return;
        }

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

    VERBOSE(VB_MEDIA, "Initial device list: " + listDevices());
}

bool MediaMonitorWindows::AddDevice(MythMediaDevice *pDevice)
{
    if (!pDevice)
    {
        VERBOSE(VB_IMPORTANT, "Error - MediaMonitorWindows::AddDevice(null)");
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
            VERBOSE(VB_MEDIA, "MediamonitorWindows::AddDevice() -- " +
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
