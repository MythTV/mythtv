#include "mythhdd.h"

/** \fn MythHDD::Get(QObject*, const char*, bool, bool)
 *  \brief Helper function used to create a new instance
 *         of a hard disk device.
 *  \param devicePath path to the device special file
 *                    representing the device.
 *  \return new MythHDD instance.
 */
MythHDD *MythHDD::Get(QObject* par, const char* devicePath, bool SuperMount,
                      bool AllowEject)
{
    return new MythHDD(par, devicePath, SuperMount, AllowEject);
}

/** \fn MythHDD::MythHDD(QObject *, const char *, bool, bool)
 *  \brief Creates a new instance of a hard disc device.
 *  \param DevicePath path to the device special file representing the device.
 *  \return new MythHDD instance.
 */
MythHDD::MythHDD(QObject *par, const char *DevicePath,
                 bool SuperMount, bool AllowEject)
    : MythMediaDevice(par, DevicePath, SuperMount, AllowEject)
{
    m_Status = MEDIASTAT_UNPLUGGED;
    m_MediaType = MEDIATYPE_DATA;       // default type is data
}

/** \fn MythHDD::checkMedia(void)
 *  \brief Checks the status of this media device.
 */
MythMediaStatus MythHDD::checkMedia(void)
{
    if (isMounted())
    {
        // A lazy way to present volume name for the user to eject.
        // Hotplug devices are usually something like /media/VOLUME
        m_VolumeID = m_MountPath;

        // device is mounted, trigger event
        return setStatus(MEDIASTAT_MOUNTED);
    }

    // device is not mounted
    if (m_Status == MEDIASTAT_UNPLUGGED)
    {
        // a removable device was just plugged in try to mount it.
        mount();
        if (isMounted())
        {
            m_Status = MEDIASTAT_NOTMOUNTED;
            return setStatus(MEDIASTAT_MOUNTED);
        }
        else
            return setStatus(MEDIASTAT_NOTMOUNTED);
    }
    else if (m_Status == MEDIASTAT_MOUNTED)
    {
        // device was mounted and someone unmounted it.
        return m_Status = setStatus(MEDIASTAT_NOTMOUNTED);
    }
    else
    {
        // leave device state as is
        return m_Status;
    }
}
