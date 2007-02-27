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


QStringList MediaMonitorWindows::GetCDROMBlockDevices()
{
    QStringList  list;


    for (each device)
        if (device.isCDorDVD())
            list.append(device.letter());

    return list;
}
