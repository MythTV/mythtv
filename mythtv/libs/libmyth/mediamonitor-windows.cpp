/**
 * \file     mediamonitor-windows.cpp
 * \brief    MythMediaMonitor for Windows
 */

#include "mythmediamonitor.h"
#include "mediamonitor-windows.h"
#include "mythcdrom.h"
#include "mythhdd.h"


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
    for (each device)
        if (device.isRemovable())
        {
            MythMediaDevice  *media;
    
            if (device.isCDorDVD())
                media = MythCDROM::get(this, device.letter(),
                                       false, AllowEject);
            else
                media = MythHDD::Get(this, device.letter(),
                                     false, allowEject);

            if (!media)
            {
                VERBOSE(VB_IMPORTANT,
                        "Error. Couldn't create MythMediaDevice.");
                return;
            }

            // We store the volume name to improve
            // user activities like ChooseAndEjectMedia().
            media->setVolumeID(device.volName());

            AddDevice(media);
        }
}

/**
 * Simplest possible device list populator. No duplicate checking
 */
bool MediaMonitorWindows::AddDevice(MythMediaDevice* pDevice)
{
    if (! pDevice)
    {
        VERBOSE(VB_IMPORTANT, "Error - MediaMonitorWndows::AddDevice(null)");
        return false;
    }

    // If the user doesn't want this device to be monitored, stop now:
    if (m_IgnoreList.contains(pDevice->getDevicePath()))
        return false;

    m_Devices.push_back(pDevice);
    m_UseCount[pDevice] = 0;

    return true;
}

QStringList MediaMonitorWindows::GetCDROMBlockDevices()
{
    QStringList  list;


    for (each device)
        if (device.isCDorDVD())
            list.append(device.letter());

    return list;
}
