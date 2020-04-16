#include "mythhdd.h"
#include "mythlogging.h"

/** \fn MythHDD::Get(QObject*, const char*, bool, bool)
 *  \brief Helper function used to create a new instance
 *         of a hard disk device.
 *  \param par        The parent object of this hdd object.
 *  \param devicePath path to the device special file
 *                    representing the device.
 *  \param SuperMount True if this is a 'supermount' device.
                      \sa MythMediaDevice::m_SuperMount
 *  \param AllowEject True if the user is allowed to eject the media.
 *  \return new MythHDD instance.
 */
MythHDD *MythHDD::Get(QObject* par, const char* devicePath, bool SuperMount,
                      bool AllowEject)
{
    return new MythHDD(par, devicePath, SuperMount, AllowEject);
}

/** \fn MythHDD::MythHDD(QObject *, const char *, bool, bool)
 *  \brief Creates a new instance of a hard disc device.
 *  \param par        The parent object of this hdd object.
 *  \param DevicePath path to the device special file representing the device.
 *  \param SuperMount True if this is a 'supermount' device.
                      \sa MythMediaDevice::m_SuperMount
 *  \param AllowEject True if the user is allowed to eject the media.
 *  \return new MythHDD instance.
 */
MythHDD::MythHDD(QObject *par, const QString& DevicePath,
                 bool SuperMount, bool AllowEject)
    : MythMediaDevice(par, DevicePath, SuperMount, AllowEject)
{
    LOG(VB_MEDIA, LOG_INFO, "MythHDD::MythHDD " + m_devicePath);
    m_status = MEDIASTAT_NOTMOUNTED;
    m_mediaType = MEDIATYPE_UNKNOWN;
}

/** \fn MythHDD::checkMedia(void)
 *  \brief Checks the status of this media device.
 */
MythMediaStatus MythHDD::checkMedia(void)
{
    if (m_status == MEDIASTAT_ERROR)
        return m_status;

    if (isMounted())
    {
        // A lazy way to present volume name for the user to eject.
        // Hotplug devices are usually something like /media/VOLUME
        m_volumeID = m_mountPath;

        // device is mounted, trigger event
        if (m_status != MEDIASTAT_MOUNTED)
        {
            m_status = MEDIASTAT_NOTMOUNTED;
            onDeviceMounted();
        }

        return setStatus(MEDIASTAT_MOUNTED);
    }

    // device is not mounted
    switch (m_status)
    {
        case MEDIASTAT_NOTMOUNTED:
            // a removable device was just plugged in try to mount it.
            LOG(VB_MEDIA, LOG_INFO, "MythHDD::checkMedia try mounting " +
                m_devicePath);

            if (mount())
                return setStatus(MEDIASTAT_MOUNTED);

            return setStatus(MEDIASTAT_ERROR);

        case MEDIASTAT_MOUNTED:
            // device was mounted and someone unmounted it.
            clearData();
            return setStatus(MEDIASTAT_NOTMOUNTED);

        default:
            // leave device state as is
            return m_status;
    }
}

//virtual
MythMediaError MythHDD::eject(bool /*open_close*/)
{
    setStatus(MEDIASTAT_UNPLUGGED);
    return MEDIAERR_UNSUPPORTED;
}
