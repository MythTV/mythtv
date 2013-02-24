
// Standard C headers
#include <cstdio>

// C++ headers
#include <iostream>
#include <typeinfo>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QFile>
#include <QList>
#include <QDir>

// MythTV headers
#include "mythmediamonitor.h"
#include "mythcdrom.h"
#include "mythcorecontext.h"
#include "mythdialogs.h"
#include "mythconfig.h"
#include "mythdialogbox.h"
#include "mythdate.h"
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythsystem.h"
#include "mythmiscutil.h"

#ifdef USING_DARWIN_DA
#include "mediamonitor-darwin.h"
#elif CONFIG_CYGWIN || defined(_WIN32)
#include "mediamonitor-windows.h"
#else
#include "mediamonitor-unix.h"
#endif

MediaMonitor *MediaMonitor::c_monitor = NULL;

// MonitorThread
MonitorThread::MonitorThread(MediaMonitor* pMon, unsigned long interval) :
    MThread("Monitor")
{
    m_Monitor = pMon;
    m_Interval = interval;
}

// Nice and simple, as long as our monitor is valid and active,
// loop and check it's devices.
void MonitorThread::run(void)
{
    RunProlog();
    while (m_Monitor && m_Monitor->IsActive())
    {
        m_Monitor->CheckDevices();
        msleep(m_Interval);
    }
    RunEpilog();
}

////////////////////////////////////////////////////////////////////////
// MediaMonitor

#define MONITOR_INTERVAL 5000

MediaMonitor* MediaMonitor::GetMediaMonitor(void)
{
    if (c_monitor)
        return c_monitor;

#ifdef USING_DARWIN_DA
    c_monitor = new MediaMonitorDarwin(NULL, MONITOR_INTERVAL, true);
#else
  #if CONFIG_CYGWIN || defined(_WIN32)
    c_monitor = new MediaMonitorWindows(NULL, MONITOR_INTERVAL, true);
  #else
    c_monitor = new MediaMonitorUnix(NULL, MONITOR_INTERVAL, true);
  #endif
#endif

    return c_monitor;
}

void MediaMonitor::SetCDSpeed(const char *device, int speed)
{
    MediaMonitor *mon = GetMediaMonitor();
    if (mon)
    {
        MythMediaDevice *pMedia = mon->GetMedia(device);
        if (pMedia && mon->ValidateAndLock(pMedia))
        {
            pMedia->setSpeed(speed);
            mon->Unlock(pMedia);
            return;
        }
    }

    MythCDROM *cd = MythCDROM::get(NULL, device, false, false);
    if (cd)
    {
        cd->setDeviceSpeed(device, speed);
        delete cd;
        return;
    }

    LOG(VB_MEDIA, LOG_INFO, 
             QString("MediaMonitor::setSpeed(%1) - Cannot find/create CDROM?")
                  .arg(device));
}

// When ejecting one of multiple devices, present a nice name to the user
static const QString DevName(MythMediaDevice *d)
{
    QString str = d->getVolumeID();  // First choice, the name of the media

    if (str.isEmpty())
    {
        str = d->getDeviceModel();   // otherwise, the drive manufacturer/model

        if (!str.isEmpty())                     // and/or the device node
            str += " (" + d->getDevicePath() + ')';
        else
            str = d->getDevicePath();
    }
    //     We could add even more information here, but volume names
    //     are usually descriptively unique (i.e. usually good enough)
    //else
    //    str += " (" + d->getDeviceModel() + ", " + d->getDevicePath() + ')';

    return str;
}

/**
 * \brief Generate a list of removable drives.
 *
 * Has to iterate through all devices to check if any are suitable.
 */
QList<MythMediaDevice*> MediaMonitor::GetRemovable(bool showMounted,
                                                   bool showUsable)
{
    QList <MythMediaDevice *>           drives;
    QList <MythMediaDevice *>::iterator it;
    QMutexLocker                        locker(&m_DevicesLock);

    for (it = m_Devices.begin(); it != m_Devices.end(); ++it)
    {
        // By default, we only list CD/DVD devices.
        // Caller can also request mounted drives to be listed (e.g. USB flash)

        if (showUsable && !(*it)->isUsable())
            continue;
        
        if (QString(typeid(**it).name()).contains("MythCDROM") ||
               (showMounted && (*it)->isMounted(false)))
            drives.append(*it);
    }

    return drives;
}

/**
 * \brief List removable drives, let the user select one.
 *
 * prevent drawing a list if there is only one drive, et cetera
 */
MythMediaDevice * MediaMonitor::selectDrivePopup(const QString label,
                                                 bool showMounted,
                                                 bool showUsable)
{
    QList <MythMediaDevice *> drives = GetRemovable(showMounted,
                                                    showUsable);

    if (drives.count() == 0)
    {
        QString msg = "MediaMonitor::selectDrivePopup() ";
        if (m_StartThread)
            msg += "- no removable devices";
        else
            msg += "MonitorDrives is disabled - no device list";

        LOG(VB_MEDIA, LOG_INFO, msg);
        return NULL;
    }

    if (drives.count() == 1)
    {
        LOG(VB_MEDIA, LOG_INFO, 
                 "MediaMonitor::selectDrivePopup(" + label +
                 ") - One suitable device");
        return drives.front();
    }

    QList <MythMediaDevice *>::iterator it;
    QStringList buttonmsgs;
    for (it = drives.begin(); it != drives.end(); ++it)
        buttonmsgs += DevName(*it);
    buttonmsgs += tr("Cancel");
    const DialogCode cancelbtn = (DialogCode)
        (((int)kDialogCodeButton0) + buttonmsgs.size() - 1);

    DialogCode ret = MythPopupBox::ShowButtonPopup(
        GetMythMainWindow(), "select drive", label,
        buttonmsgs, cancelbtn);

    // If the user cancelled, return a special value
    if ((kDialogCodeRejected == ret) || (cancelbtn == ret))
        return (MythMediaDevice *)-1;

    int idx = MythDialog::CalcItemIndex(ret);
    if (idx < drives.count())
        return drives[idx];

    return NULL;
}


/** \fn MediaMonitor::ChooseAndEjectMedia(void)
 *  \brief Unmounts and ejects removable media devices.
 *
 *  If no media devices are known to the MediaMonitor this function does
 *  nothing. If a single device is known, it is unmounted and ejected if
 *  possible. If multiple devices are known, a popup box is display so the
 *  user can choose which device to eject.
 */
void MediaMonitor::ChooseAndEjectMedia(void)
{
    MythMediaDevice *selected;


    selected = selectDrivePopup(tr("Select removable media"
                                   " to eject or insert"), true);

    // If the user cancelled, no need to display or do anything more
    if (selected == (MythMediaDevice *) -1)
        return;

    if (!selected)
    {
        ShowOkPopup(tr("No devices to eject"));
        return;
    }

    AttemptEject(selected);
}

void MediaMonitor::AttemptEject(MythMediaDevice *device)
{
    QString  dev = DevName(device);

    if (device->getStatus() == MEDIASTAT_OPEN)
    {
        LOG(VB_MEDIA, LOG_INFO,
                 QString("Disk %1's tray is OPEN. Closing tray").arg(dev));

        if (device->eject(false) != MEDIAERR_OK)
        {
            QString msg = QObject::tr(
                "Unable to open or close the empty drive %1.\n\n"
                "You may have to use the eject button under its tray.");
            ShowOkPopup(msg.arg(dev));
        }

        return;
    }

    if (device->isMounted())
    {
        LOG(VB_MEDIA, LOG_INFO,
                 QString("Disk %1 is mounted? Unmounting").arg(dev));
        device->unmount();

        if (device->isMounted())
        {
            ShowOkPopup(tr("Failed to unmount %1").arg(dev));
            return;
        }
    }

    LOG(VB_MEDIA, LOG_INFO,
             QString("Unlocking disk %1, then ejecting").arg(dev));
    device->unlock();

    MythMediaError err = device->eject();

    if (err == MEDIAERR_UNSUPPORTED)
    {
        // Physical ejection isn't possible (there is no tray or slot),
        // but logically the device is now ejected (ignored by the OS).
        ShowOkPopup(tr("You may safely remove %1").arg(dev));
    }
    else if (err == MEDIAERR_FAILED)
    {
        ShowOkPopup(tr("Failed to eject %1").arg(dev));
    }
}

/**
 * \brief  Lookup some settings, and do OS-specific stuff in sub-classes
 *
 * \bug    If the user changes the MonitorDrives or IgnoreDevices settings,
 *         it will have no effect until the frontend is restarted.
 */
MediaMonitor::MediaMonitor(QObject* par, unsigned long interval,
                           bool allowEject)
    : QObject(par), m_Active(false), m_Thread(NULL),
      m_MonitorPollingInterval(interval), m_AllowEject(allowEject)
{
    // MediaMonitor object is always created,
    // but the user can elect not to actually do monitoring:
    m_StartThread = gCoreContext->GetNumSetting("MonitorDrives");

    // User can specify that some devices are not monitored
    QString ignore = gCoreContext->GetSetting("IgnoreDevices", "");

    if (ignore.length())
        m_IgnoreList = ignore.split(',', QString::SkipEmptyParts);
    else
        m_IgnoreList = QStringList();  // Force empty list

    if (m_StartThread)
        LOG(VB_MEDIA, LOG_NOTICE, "Creating MediaMonitor");
    else
#ifdef USING_DARWIN_DA
        LOG(VB_MEDIA, LOG_INFO,
                 "MediaMonitor is disabled. Eject will not work");
#else
        LOG(VB_MEDIA, LOG_INFO,
                 "Creating inactive MediaMonitor and static device list");
#endif
    LOG(VB_MEDIA, LOG_INFO, "IgnoreDevices=" + ignore);

    // If any of IgnoreDevices are symlinks, also add the real device
    QStringList::Iterator dev;
    for (dev = m_IgnoreList.begin(); dev != m_IgnoreList.end(); ++dev)
    {
        QFileInfo *fi = new QFileInfo(*dev);

        if (fi && fi->isSymLink())
        {
            QString target = getSymlinkTarget(*dev);

            if (m_IgnoreList.filter(target).isEmpty())
            {
                LOG(VB_MEDIA, LOG_INFO,
                         "Also ignoring " + target + " (symlinked from " + 
                         *dev + ").");
                m_IgnoreList += target;
            }
        }
        delete fi;
    }
}

void MediaMonitor::deleteLater(void)
{
    if (m_Thread)
    {
        StopMonitoring();
        delete m_Thread;
        m_Thread = NULL;
    }
    QObject::deleteLater();
}

/** \fn MediaMonitor::RemoveDevice(const QString&)
 *  \brief Remove a device from the media monitor.
 *
 *   This function is usually called after a hotpluggable device is unplugged.
 *
 *  \param dev path to device special file to remove.
 *  \return true if device is removed from the Media Monitor.
 */
bool MediaMonitor::RemoveDevice(const QString &dev)
{
    QMutexLocker locker(&m_DevicesLock);

    QList<MythMediaDevice*>::iterator it;
    for (it = m_Devices.begin(); it != m_Devices.end(); ++it)
    {
        if ((*it)->getDevicePath() == dev)
        {
            if (m_UseCount[*it] == 0)
            {
                m_UseCount.remove(*it);
                (*it)->deleteLater();
                m_Devices.erase(it);
            }
            else
            {
                // Other threads are still using this device
                // postpone actual delete until they finish.
                disconnect(*it);
                m_RemovedDevices.append(*it);
                m_Devices.erase(it);
            }

            return true;
        }
    }
    return false;
}

/** \fn MediaMonitor::CheckDevices(void)
 *  \brief Poll the devices in our list.
 */
void MediaMonitor::CheckDevices(void)
{
    /* check if new devices have been plugged in */
    CheckDeviceNotifications();

    QList<MythMediaDevice*>::iterator itr = m_Devices.begin();
    MythMediaDevice* pDev;
    while (itr != m_Devices.end())
    {
        pDev = *itr;
        if (pDev)
            pDev->checkMedia();
        ++itr;
    }
}

/** \fn MediaMonitor::StartMonitoring(void)
 *  \brief Start the monitoring thread if needed.
 */
void MediaMonitor::StartMonitoring(void)
{
    // Sanity check
    if (m_Active)
        return;

    if (!m_StartThread)
        return;

    if (!m_Thread)
        m_Thread = new MonitorThread(this, m_MonitorPollingInterval);

    qRegisterMetaType<MythMediaStatus>("MythMediaStatus");

    LOG(VB_MEDIA, LOG_NOTICE, "Starting MediaMonitor");
    m_Active = true;
    m_Thread->start();
}

/** \fn MediaMonitor::StopMonitoring(void)
 *  \brief Stop the monitoring thread if needed.
 */
void MediaMonitor::StopMonitoring(void)
{
    // Sanity check
    if (!m_Active)
        return;

    LOG(VB_MEDIA, LOG_NOTICE, "Stopping MediaMonitor");
    m_Active = false;
    m_Thread->wait();
}

/** \fn MediaMonitor::ValidateAndLock(MythMediaDevice *pMedia)
 *  \brief Validates the MythMediaDevice and increments its reference count
 *
 *   Returns true if pMedia device is valid, otherwise returns false.
 *   If this function returns false the caller should gracefully recover.
 *
 *   NOTE: This function can block.
 *
 *  \sa Unlock(MythMediaDevice *pMedia), GetMedias(MythMediaType mediatype)
 */
bool MediaMonitor::ValidateAndLock(MythMediaDevice *pMedia)
{
    QMutexLocker locker(&m_DevicesLock);

    if (!m_Devices.contains(pMedia))
        return false;

    m_UseCount[pMedia]++;

    return true;
}

/** \fn MediaMonitor::Unlock(MythMediaDevice *pMedia)
 *  \brief decrements the MythMediaDevices reference count
 *
 *  \sa ValidateAndLock(MythMediaDevice *pMedia), GetMedias(MythMediaType mediatype)
 */
void MediaMonitor::Unlock(MythMediaDevice *pMedia)
{
    QMutexLocker locker(&m_DevicesLock);

    if (!m_UseCount.contains(pMedia))
        return;

    m_UseCount[pMedia]--;

    if (m_UseCount[pMedia] == 0 && m_RemovedDevices.contains(pMedia))
    {
        m_RemovedDevices.removeAll(pMedia);
        m_UseCount.remove(pMedia);
        pMedia->deleteLater();
    }
}

/**  \fn MediaMonitor::GetMedia(const QString& path)
 * \brief Get media device by pathname. Must be locked with ValidateAndLock().
 *
 *  \sa ValidateAndLock(MythMediaDevice *pMedia)
 *  \sa Unlock(MythMediaDevice *pMedia)
 */
MythMediaDevice* MediaMonitor::GetMedia(const QString& path)
{
    QMutexLocker locker(&m_DevicesLock);

    QList<MythMediaDevice*>::iterator it = m_Devices.begin();
    for (;it != m_Devices.end(); ++it)
    {
        if ((*it)->isSameDevice(path) &&
            (((*it)->getStatus() == MEDIASTAT_USEABLE) ||
             ((*it)->getStatus() == MEDIASTAT_MOUNTED) ||
             ((*it)->getStatus() == MEDIASTAT_NOTMOUNTED)))
        {
            return(*it);
        }
    }

    return NULL;
}

/**
 * If the device is being monitored, return its mountpoint.
 *
 * A convenience function for plugins.
 * (Only currently needed for Mac OS X, which mounts Audio CDs)
 */
QString MediaMonitor::GetMountPath(const QString& devPath)
{
    QString mountPath;

    if (c_monitor)
    {
        MythMediaDevice *pMedia = c_monitor->GetMedia(devPath);
        if (pMedia && c_monitor->ValidateAndLock(pMedia))
        {
            mountPath = pMedia->getMountPath();
            c_monitor->Unlock(pMedia);
        }
        // The media monitor could be inactive.
        // Create a fake media device just to lookup mount map:
        else
        {
            pMedia = MythCDROM::get(NULL, devPath.toLatin1(), true, false);
            if (pMedia && pMedia->findMountPath())
                mountPath = pMedia->getMountPath();
            else
                LOG(VB_MEDIA, LOG_INFO,
                         "MediaMonitor::GetMountPath() - failed");
            // need some way to delete the media device.
        }
    }

    return mountPath;
}

/** \fn MediaMonitor::GetMedias(MythMediaType mediatype)
 *  \brief Ask for available media. Must be locked with ValidateAndLock().
 *
 *   This method returns a list of MythMediaDevice pointers which match
 *   the given mediatype.
 *
 *   It is potentially unsafe to use the pointers returned by
 *   this function. The devices may be removed and their
 *   associated MythMediaDevice objects destroyed. It is the
 *   responsibility of the caller to ensure that the pointers
 *   are validated and the reference count is incremented by
 *   calling MediaMonitor::ValidateAndLock() before the the
 *   returned pointer is dereferenced and MediaMonitor::Unlock()
 *   when done.
 *
 *  \sa ValidateAndLock(MythMediaDevice *pMedia)
 *  \sa Unlock(MythMediaDevice *pMedia)
 */
QList<MythMediaDevice*> MediaMonitor::GetMedias(MythMediaType mediatype)
{
    QMutexLocker locker(&m_DevicesLock);

    QList<MythMediaDevice*> medias;

    QList<MythMediaDevice*>::iterator it = m_Devices.begin();
    for (;it != m_Devices.end(); ++it)
    {
        if (((*it)->getMediaType() & mediatype) &&
            (((*it)->getStatus() == MEDIASTAT_USEABLE) ||
             ((*it)->getStatus() == MEDIASTAT_MOUNTED) ||
             ((*it)->getStatus() == MEDIASTAT_NOTMOUNTED)))
        {
            medias.push_back(*it);
        }
    }

    return medias;
}

/** \fn MediaMonitor::MonitorRegisterExtensions(uint,const QString&)
 *  \brief Register the extension list on all known devices
 */
void MediaMonitor::MonitorRegisterExtensions(uint mediatype,
                                             const QString &extensions)
{
    LOG(VB_GENERAL, LOG_DEBUG, 
             QString("MonitorRegisterExtensions(0x%1, %2)")
                 .arg(mediatype, 0, 16).arg(extensions));

    QList<MythMediaDevice*>::iterator it = m_Devices.begin();
    for (; it != m_Devices.end(); ++it)
    {
        if (*it)
            (*it)->RegisterMediaExtensions(mediatype, extensions);
    }
}

void MediaMonitor::RegisterMediaHandler(const QString  &destination,
                                        const QString  &description,
                                        const QString  &key,
                                        void          (*callback)
                                              (MythMediaDevice*),
                                        int             mediaType,
                                        const QString  &extensions)
{
    if (m_handlerMap.count(destination) == 0)
    {
        MHData  mhd = { callback, mediaType, destination, description };
        QString msg = MythMediaDevice::MediaTypeString((MythMediaType)mediaType);

        if (extensions.length())
            msg += QString(", ext(%1)").arg(extensions);

        LOG(VB_MEDIA, LOG_INFO, 
                 "Registering '" + destination + "' as a media handler for " +
                 msg);

        m_handlerMap[destination] = mhd;

        if (extensions.length())
            MonitorRegisterExtensions(mediaType, extensions);
    }
    else
    {
       LOG(VB_GENERAL, LOG_INFO,
                destination + " is already registered as a media handler.");
    }
}

/**
 * Find a relevant jump point for this type of media.
 *
 * If there's more than one we should show a popup
 * to allow the user to select which one to use,
 * but for now, we're going to just use the first one.
 */
void MediaMonitor::JumpToMediaHandler(MythMediaDevice* pMedia)
{
    QList<MHData>                    handlers;
    QMap<QString, MHData>::Iterator  itr = m_handlerMap.begin();

    while (itr != m_handlerMap.end())
    {
        if (((*itr).MythMediaType & (int)pMedia->getMediaType()))
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Found a handler for %1 - '%2'")
                .arg(pMedia->MediaTypeString()) .arg(itr.key())); 
            handlers.append(*itr);
        }
        itr++;
    }

    if (handlers.empty())
    {
        LOG(VB_MEDIA, LOG_INFO, "No media handler found for event type");
        return;
    }


    // TODO - Generate a dialog, add buttons for each description,
    // if user didn't cancel, selected = handlers.at(choice);
    int selected = 0;

    handlers.at(selected).callback(pMedia);
}

/**
 * \brief Slot which is called when the device status changes and posts a
 *        media event to the mainwindow
 */
void MediaMonitor::mediaStatusChanged(MythMediaStatus oldStatus,
                                      MythMediaDevice* pMedia)
{
    // If we're not active then ignore signal.
    if (!m_Active)
        return;

    MythMediaStatus  stat = pMedia->getStatus();
    QString      msg  = QString(" (%1, %2 -> %3)")
                        .arg(pMedia->MediaTypeString())
                        .arg(MythMediaDevice::MediaStatusStrings[oldStatus])
                        .arg(MythMediaDevice::MediaStatusStrings[stat]);

    // This gets called from outside the main thread so we need
    // to post an event back to the main thread.
    // We now send events for all non-error statuses, so plugins get ejects
    if (stat != MEDIASTAT_ERROR && stat != MEDIASTAT_UNKNOWN)
    {
        // Should we ValidateAndLock() first?
        QEvent *e = new MythMediaEvent(stat, pMedia);

        LOG(VB_MEDIA, LOG_INFO, "Posting MediaEvent" + msg);

        // sendEvent() is needed here - it waits for the event to be used.
        // postEvent() would result in pDevice's media type changing
        // ... before the plugin's event chain would process it.
        // Another way would be to send an exact copy of pDevice instead.
        QCoreApplication::sendEvent((QObject*)GetMythMainWindow(), e);
        delete e;
    }
    else
        LOG(VB_MEDIA, LOG_INFO,
                 "Media status changed, but not sending event" + msg);


    if (stat == MEDIASTAT_OPEN || stat == MEDIASTAT_NODISK
                               || stat == MEDIASTAT_UNPLUGGED)
    {
        pMedia->clearData();
    }
}

/**
 * Check user preferences to see if this device should be monitored
 */
bool MediaMonitor::shouldIgnore(const MythMediaDevice* device)
{
    if (m_IgnoreList.contains(device->getMountPath()) ||
        m_IgnoreList.contains(device->getRealDevice())||
        m_IgnoreList.contains(device->getDevicePath()) )
    {
        LOG(VB_MEDIA, LOG_INFO,
                 "Ignoring device: " + device->getDevicePath());
        return true;
    }
#if 0
    else
    {
        LOG(VB_MEDIA, LOG_DEBUG,
                 "Not ignoring: " + device->getDevicePath() + " / " +
                 device->getMountPath());
        LOG(VB_MEDIA, LOG_DEBUG, 
                 "Paths not in: " + m_IgnoreList.join(", "));
    }
#endif

    return false;
}

/**
 * Installed into the main window's event chain,
 * so that the main thread can safely jump to plugin code.
 */
bool MediaMonitor::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == MythMediaEvent::kEventType)
    {
        MythMediaDevice *pDev = ((MythMediaEvent*)event)->getDevice();

        if (!pDev)
        {
            LOG(VB_GENERAL, LOG_ALERT,
                     "MediaMonitor::eventFilter() got a bad media event?");
            return true;
        }

        if (pDev->isUsable())
            JumpToMediaHandler(pDev);
        else
        {
            // We don't want to jump around in the menus, but should
            // call each plugin's callback so it can track this change.

            QMap<QString, MHData>::Iterator itr = m_handlerMap.begin();
            while (itr != m_handlerMap.end())
            {
                if ((*itr).MythMediaType & (int)pDev->getMediaType())
                    (*itr).callback(pDev);
                itr++;
            }
        }

        return false;  // Don't eat the event
    }

    // standard event processing
    return QObject::eventFilter(obj, event);
}

/*
 * These methods return the user's preferred devices for playing and burning
 * CDs and DVDs. Traditionally we had a database setting to remember this,
 * but that is a bit wasteful when most users only have one drive.
 *
 * To make it a bit more beginner friendly, if no database default exists,
 * or if it contains "default", the code tries to find a monitored drive.
 * If, still, nothing is suitable, a caller hard-coded default is used.
 *
 * Ideally, these would return a MythMediaDevice * instead of a QString
 */

QString MediaMonitor::defaultDevice(QString dbSetting,
                                    QString label,
                                    const char *hardCodedDefault)
{
    QString device = gCoreContext->GetSetting(dbSetting);

    LOG(VB_MEDIA, LOG_DEBUG,
             QString("MediaMonitor::defaultDevice(%1,..,%2) dbSetting='%3'")
                 .arg(dbSetting).arg(hardCodedDefault).arg(device));

    // No settings database defaults? Try to choose one:
    if (device.isEmpty() || device == "default")
    {
        device = hardCodedDefault;

        if (!c_monitor)
            c_monitor = GetMediaMonitor();

        if (c_monitor)
        {
            MythMediaDevice *d = c_monitor->selectDrivePopup(label, false, true);

            if (d == (MythMediaDevice *) -1)    // User cancelled
            {
                device.clear(); // If user has explicitly cancelled return empty string
                d = NULL;
            }

            if (d && c_monitor->ValidateAndLock(d))
            {
                device = d->getDevicePath();
                c_monitor->Unlock(d);
            }
        }
    }

    LOG(VB_MEDIA, LOG_DEBUG,
             "MediaMonitor::defaultDevice() returning " + device);
    return device;
}

/**
 * CDDevice, user-selected drive, or /dev/cdrom
 */
QString MediaMonitor::defaultCDdevice()
{
    return defaultDevice("CDDevice", tr("Select a CD drive"), DEFAULT_CD);
}

/**
 * VCDDeviceLocation, user-selected drive, or /dev/cdrom
 */
QString MediaMonitor::defaultVCDdevice()
{
    return defaultDevice("VCDDeviceLocation",
                         tr("Select a VCD drive"), DEFAULT_CD);
}

/**
 * DVDDeviceLocation, user-selected drive, or /dev/dvd
 */
QString MediaMonitor::defaultDVDdevice()
{
    return defaultDevice("DVDDeviceLocation",
                         tr("Select a DVD drive"), DEFAULT_DVD);
}

/**
 * \brief CDWriterDeviceLocation, user-selected drive, or /dev/cdrom
 */
QString MediaMonitor::defaultCDWriter()
{
    return defaultDevice("CDWriterDeviceLocation",
                         tr("Select a CD writer"), DEFAULT_CD);
}

/**
 * \brief MythArchiveDVDLocation, user-selected drive, or /dev/dvd
 *
 * This should also look for drives with blanks or RWs in them,
 * but Nigel hasn't worked out how to do this tidily (yet).
 */
QString MediaMonitor::defaultDVDWriter()
{
    QString device = defaultDevice("MythArchiveDVDLocation",
                                   tr("Select a DVD writer"), DEFAULT_DVD);

    return device;
}


/**
 * \brief A string summarising the current devices, for debugging
 */
const QString MediaMonitor::listDevices(void)
{
    QList<MythMediaDevice*>::const_iterator dev;
    QStringList list;

    for (dev = m_Devices.begin(); dev != m_Devices.end(); ++dev)
    {
        QString devStr;
        QString model = (*dev)->getDeviceModel();
        QString path  = (*dev)->getDevicePath();
        QString real  = (*dev)->getRealDevice();

        if (path != real)
            devStr += path + "->";
        devStr += real;

        if (!model.length())
            model = "unknown";
        devStr += " (" + model + ")";

        list += devStr;
    }

    return list.join(", ");
}

/**
 * \brief Eject a disk, unmount a drive, open a tray
 *
 * If the Media Monitor is enabled, we use its fully-featured routine.
 * Otherwise, we guess a drive and use a primitive OS-specific command
 */
void MediaMonitor::ejectOpticalDisc()
{
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon)
        mon->ChooseAndEjectMedia();
    else
    {
        LOG(VB_MEDIA, LOG_INFO, "CD/DVD Monitor isn't enabled.");
#ifdef __linux__
        LOG(VB_MEDIA, LOG_INFO, "Trying Linux 'eject -T' command");
        myth_system("eject -T");
#elif CONFIG_DARWIN
        QString def = DEFAULT_CD;
        LOG(VB_MEDIA, LOG_INFO, "Trying 'disktool -e " + def);
        myth_system("disktool -e " + def);
#endif
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
