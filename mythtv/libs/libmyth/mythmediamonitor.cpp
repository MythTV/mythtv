
// Standard C headers
#include <cstdio>

// C++ headers
#include <iostream>
#include <typeinfo>

// Qt headers
#include <QtGlobal>
#include <QCoreApplication>
#include <QFile>
#include <QList>
#include <QDir>

// MythTV headers
#include "mythmediamonitor.h"
#include "mythcdrom.h"
#include "mythcorecontext.h"
#include "mythconfig.h"
#include "mythdialogbox.h"
#include "mythdate.h"
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythsystemlegacy.h"
#include "mythmiscutil.h"

#ifdef USING_DARWIN_DA
#include "mediamonitor-darwin.h"
#elif defined(Q_OS_WIN)
#include "mediamonitor-windows.h"
#else
#include "mediamonitor-unix.h"
#endif

static const QString sLocation = QObject::tr("Media Monitor");

MediaMonitor *MediaMonitor::s_monitor = nullptr;

// MonitorThread
MonitorThread::MonitorThread(MediaMonitor* pMon, unsigned long interval) :
    MThread("Monitor")
{
    m_monitor = pMon;
    m_interval = interval;
    m_lastCheckTime = QDateTime::currentDateTimeUtc();
}

// Nice and simple, as long as our monitor is valid and active,
// loop and check it's devices.
void MonitorThread::run(void)
{
    RunProlog();
    QMutex mtx;
    mtx.lock();
    while (m_monitor && m_monitor->IsActive())
    {
        m_monitor->CheckDevices();
        m_monitor->m_wait.wait(&mtx, m_interval);
        QDateTime now(QDateTime::currentDateTimeUtc());
        // if 10 seconds have elapsed instead of 5 seconds
        // assume the system was suspended and reconnect
        // sockets
        if (m_lastCheckTime.secsTo(now) > 120)
        {
            gCoreContext->ResetSockets();
            if (HasMythMainWindow())
            {
                LOG(VB_GENERAL, LOG_INFO, "Restarting LIRC handler");
                GetMythMainWindow()->RestartInputHandlers();
            }
        }
        m_lastCheckTime = now;
    }
    mtx.unlock();
    RunEpilog();
}

////////////////////////////////////////////////////////////////////////
// MediaMonitor

#define MONITOR_INTERVAL 5000

MediaMonitor* MediaMonitor::GetMediaMonitor(void)
{
    if (s_monitor)
        return s_monitor;

#ifdef USING_DARWIN_DA
    s_monitor = new MediaMonitorDarwin(nullptr, MONITOR_INTERVAL, true);
#elif defined(Q_OS_WIN)
    s_monitor = new MediaMonitorWindows(nullptr, MONITOR_INTERVAL, true);
#else
    s_monitor = new MediaMonitorUnix(nullptr, MONITOR_INTERVAL, true);
#endif

    return s_monitor;
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

    MythCDROM *cd = MythCDROM::get(nullptr, device, false, false);
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
static QString DevName(MythMediaDevice *d)
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
    QMutexLocker                        locker(&m_devicesLock);

    for (it = m_devices.begin(); it != m_devices.end(); ++it)
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
MythMediaDevice * MediaMonitor::selectDrivePopup(const QString &label,
                                                 bool showMounted,
                                                 bool showUsable)
{
    QList <MythMediaDevice *> drives = GetRemovable(showMounted,
                                                    showUsable);

    if (drives.count() == 0)
    {
        QString msg = "MediaMonitor::selectDrivePopup() - no removable devices";

        LOG(VB_MEDIA, LOG_INFO, msg);
        return nullptr;
    }

    if (drives.count() == 1)
    {
        LOG(VB_MEDIA, LOG_INFO,
                 "MediaMonitor::selectDrivePopup(" + label +
                 ") - One suitable device");
        return drives.front();
    }

    MythMainWindow *win = GetMythMainWindow();
    if (!win)
        return nullptr;

    MythScreenStack *stack = win->GetMainStack();
    if (!stack)
        return nullptr;

    // Ignore MENU dialog actions
    int btnIndex = -2;
    while (btnIndex < -1)
    {
        auto *dlg = new MythDialogBox(label, stack, "select drive");
        if (!dlg->Create())
        {
            delete dlg;
            return nullptr;
        }

        // Add button for each drive
        for (auto *drive : drives)
            dlg->AddButton(DevName(drive));

        dlg->AddButton(tr("Cancel"));

        stack->AddScreen(dlg);

        // Wait in local event loop so events are processed
        QEventLoop block;
        connect(dlg,    &MythDialogBox::Closed,
                &block, [&](const QString& /*resultId*/, int result) { block.exit(result); });

        // Block until dialog closes
        btnIndex = block.exec();
    }

    // If the user cancelled, return a special value
    return btnIndex < 0 || btnIndex >= drives.size() ? (MythMediaDevice *)-1
                                                     : drives.at(btnIndex);
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
    MythMediaDevice *selected =
        selectDrivePopup(tr("Select removable media to eject or insert"), true);

    // If the user cancelled, no need to display or do anything more
    if (selected == (MythMediaDevice *) -1)
        return;

    if (!selected)
    {
        ShowNotification(tr("No devices to eject"), sLocation);
        return;
    }

    AttemptEject(selected);
}


void MediaMonitor::EjectMedia(const QString &path)
{
    MythMediaDevice *device = GetMedia(path);
    if (device)
        AttemptEject(device);
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
            QString msg =
                QObject::tr("Unable to open or close the empty drive %1");
            QString extra =
                QObject::tr("You may have to use the eject button under its tray");
            ShowNotificationError(msg.arg(dev), sLocation, extra);
        }
        return;
    }

    if (device->isMounted())
    {
        LOG(VB_MEDIA, LOG_INFO,
                 QString("Disk %1 is mounted? Unmounting").arg(dev));
        device->unmount();

#ifndef Q_OS_DARWIN
        if (device->isMounted())
        {
            ShowNotificationError(tr("Failed to unmount %1").arg(dev),
                                  sLocation);
            return;
        }
#endif
    }

    LOG(VB_MEDIA, LOG_INFO,
             QString("Unlocking disk %1, then ejecting").arg(dev));
    device->unlock();

    MythMediaError err = device->eject();

    if (err == MEDIAERR_UNSUPPORTED)
    {
        // Physical ejection isn't possible (there is no tray or slot),
        // but logically the device is now ejected (ignored by the OS).
        ShowNotification(tr("You may safely remove %1").arg(dev), sLocation);
    }
    else if (err == MEDIAERR_FAILED)
    {
        ShowNotificationError(tr("Failed to eject %1").arg(dev), sLocation);
    }
}

/**
 * \brief  Lookup some settings, and do OS-specific stuff in sub-classes
 *
 * \bug    If the user changes the MonitorDrives or IgnoreDevices settings,
 *         it will have no effect until the frontend is restarted.
 */
MediaMonitor::MediaMonitor(QObject* par, unsigned long interval, bool allowEject)
  : QObject(par),
    m_monitorPollingInterval(interval),
    m_allowEject(allowEject)
{
    // User can specify that some devices are not monitored
    QString ignore = gCoreContext->GetSetting("IgnoreDevices", "");

    if (!ignore.isEmpty())
    {
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
        m_ignoreList = ignore.split(',', QString::SkipEmptyParts);
#else
        m_ignoreList = ignore.split(',', Qt::SkipEmptyParts);
#endif
    }

    LOG(VB_MEDIA, LOG_NOTICE, "Creating MediaMonitor");
    LOG(VB_MEDIA, LOG_INFO, "IgnoreDevices=" + ignore);

    // If any of IgnoreDevices are symlinks, also add the real device
    QStringList symlinked;
    for (const auto & ignored : qAsConst(m_ignoreList))
    {
        if (auto fi = QFileInfo(ignored); fi.isSymLink())
        {
            if (auto target = getSymlinkTarget(ignored); m_ignoreList.filter(target).isEmpty())
            {
                symlinked += target;
                LOG(VB_MEDIA, LOG_INFO, QString("Also ignoring %1 (symlinked from %2)")
                    .arg(target, ignored));
            }
        }
    }

    m_ignoreList += symlinked;
}

void MediaMonitor::deleteLater(void)
{
    if (m_thread)
    {
        StopMonitoring();
        delete m_thread;
        m_thread = nullptr;
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
    QMutexLocker locker(&m_devicesLock);

    QList<MythMediaDevice*>::iterator it;
    for (it = m_devices.begin(); it != m_devices.end(); ++it)
    {
        if ((*it)->getDevicePath() == dev)
        {
            // Ensure device gets an unmount
            (*it)->checkMedia();

            if (m_useCount[*it] == 0)
            {
                m_useCount.remove(*it);
                (*it)->deleteLater();
                m_devices.erase(it);
            }
            else
            {
                // Other threads are still using this device
                // postpone actual delete until they finish.
                disconnect(*it);
                m_removedDevices.append(*it);
                m_devices.erase(it);
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

    QMutexLocker locker(&m_devicesLock);

    QList<MythMediaDevice*>::iterator itr = m_devices.begin();
    while (itr != m_devices.end())
    {
        MythMediaDevice* pDev = *itr;
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
    if (m_active)
        return;
    if (!gCoreContext->GetBoolSetting("MonitorDrives", false)) {
        LOG(VB_MEDIA, LOG_NOTICE, "MediaMonitor disabled by user setting.");
        return;
    }

    if (!m_thread)
        m_thread = new MonitorThread(this, m_monitorPollingInterval);

    qRegisterMetaType<MythMediaStatus>("MythMediaStatus");

    LOG(VB_MEDIA, LOG_NOTICE, "Starting MediaMonitor");
    m_active = true;
    m_thread->start();
}

/** \fn MediaMonitor::StopMonitoring(void)
 *  \brief Stop the monitoring thread if needed.
 */
void MediaMonitor::StopMonitoring(void)
{
    // Sanity check
    if (!m_active)
        return;

    LOG(VB_MEDIA, LOG_NOTICE, "Stopping MediaMonitor");
    m_active = false;
    m_wait.wakeAll();
    m_thread->wait();
    LOG(VB_MEDIA, LOG_NOTICE, "Stopped MediaMonitor");
}

/** \fn MediaMonitor::ValidateAndLock(MythMediaDevice *pMedia)
 *  \brief Validates the MythMediaDevice and increments its reference count
 *
 *   Returns true if pMedia device is valid, otherwise returns false.
 *   If this function returns false the caller should gracefully recover.
 *
 *   NOTE: This function can block.
 *
 *  \sa Unlock(MythMediaDevice *pMedia), GetMedias(unsigned mediatypes)
 */
bool MediaMonitor::ValidateAndLock(MythMediaDevice *pMedia)
{
    QMutexLocker locker(&m_devicesLock);

    if (!m_devices.contains(pMedia))
        return false;

    m_useCount[pMedia]++;

    return true;
}

/** \fn MediaMonitor::Unlock(MythMediaDevice *pMedia)
 *  \brief decrements the MythMediaDevices reference count
 *
 *  \sa ValidateAndLock(MythMediaDevice *pMedia), GetMedias(unsigned mediatypes)
 */
void MediaMonitor::Unlock(MythMediaDevice *pMedia)
{
    QMutexLocker locker(&m_devicesLock);

    if (!m_useCount.contains(pMedia))
        return;

    m_useCount[pMedia]--;

    if (m_useCount[pMedia] == 0 && m_removedDevices.contains(pMedia))
    {
        m_removedDevices.removeAll(pMedia);
        m_useCount.remove(pMedia);
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
    QMutexLocker locker(&m_devicesLock);

    for (auto *dev : qAsConst(m_devices))
    {
        if (dev->isSameDevice(path) &&
            ((dev->getStatus() == MEDIASTAT_USEABLE) ||
             (dev->getStatus() == MEDIASTAT_MOUNTED) ||
             (dev->getStatus() == MEDIASTAT_NOTMOUNTED)))
        {
            return dev;
        }
    }

    return nullptr;
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

    if (s_monitor)
    {
        MythMediaDevice *pMedia = s_monitor->GetMedia(devPath);
        if (pMedia && s_monitor->ValidateAndLock(pMedia))
        {
            mountPath = pMedia->getMountPath();
            s_monitor->Unlock(pMedia);
        }
        // The media monitor could be inactive.
        // Create a fake media device just to lookup mount map:
        else
        {
            pMedia = MythCDROM::get(nullptr, devPath.toLatin1(), true, false);
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

/** \fn MediaMonitor::GetMedias(unsigned mediatypes)
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
QList<MythMediaDevice*> MediaMonitor::GetMedias(unsigned mediatypes)
{
    QMutexLocker locker(&m_devicesLock);

    QList<MythMediaDevice*> medias;

    for (auto *dev : qAsConst(m_devices))
    {
        if ((dev->getMediaType() & mediatypes) &&
            ((dev->getStatus() == MEDIASTAT_USEABLE) ||
             (dev->getStatus() == MEDIASTAT_MOUNTED) ||
             (dev->getStatus() == MEDIASTAT_NOTMOUNTED)))
        {
            medias.push_back(dev);
        }
    }

    return medias;
}

/** \fn MediaMonitor::RegisterMediaHandler
 *  \brief Register a handler for media related events.
 *
 *  This method registers a callback function for when media related
 *  events occur. The call must specify the type of media supported by
 *  the handler, and (if needed) a list of media file extensions that
 *  are supported.
 *
 *  \param destination A name for this callback function. For example:
 *                     "MythDVD DVD Media Handler".  This argument
 *                     must be unique as it is also used as the key
 *                     for creating the map of handler.
 *  \param description Unused.
 *  \param callback    The function to call when an event occurs.
 *  \param mediaType   The type of media supported by this callback. The
 *                     value must be an enum of type MythMediaType.
 *  \param extensions A list of file name extensions supported by this
 *  callback.
 */
void MediaMonitor::RegisterMediaHandler(const QString  &destination,
                                        const QString  &description,
                                        void          (*callback)
                                              (MythMediaDevice*),
                                        int             mediaType,
                                        const QString  &extensions)
{
    if (m_handlerMap.count(destination) == 0)
    {
        MHData  mhd = { callback, mediaType, destination, description };
        QString msg = MythMediaDevice::MediaTypeString((MythMediaType)mediaType);

        if (!extensions.isEmpty())
            msg += QString(", ext(%1)").arg(extensions);

        LOG(VB_MEDIA, LOG_INFO,
                 "Registering '" + destination + "' as a media handler for " +
                 msg);

        m_handlerMap[destination] = mhd;

        if (!extensions.isEmpty())
            MythMediaDevice::RegisterMediaExtensions(mediaType, extensions);
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
    QVector<MHData>                  handlers;
    QMap<QString, MHData>::Iterator  itr = m_handlerMap.begin();

    while (itr != m_handlerMap.end())
    {
        if (((*itr).MythMediaType & (int)pMedia->getMediaType()))
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Found a handler for %1 - '%2'")
                .arg(pMedia->MediaTypeString(), itr.key()));
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
                                      MythMediaDevice* pMedia) const
{
    // If we're not active then ignore signal.
    if (!m_active)
        return;

    MythMediaStatus  stat = pMedia->getStatus();
    QString      msg  = QString(" (%1, %2 -> %3)")
                        .arg(pMedia->MediaTypeString(),
                             MythMediaDevice::kMediaStatusStrings[oldStatus],
                             MythMediaDevice::kMediaStatusStrings[stat]);

    // This gets called from outside the main thread so we need
    // to post an event back to the main thread.
    // We now send events for all non-error statuses, so plugins get ejects
    if (stat != MEDIASTAT_ERROR && stat != MEDIASTAT_UNKNOWN &&
        // Don't send an event for a new device that's not mounted
        !(oldStatus == MEDIASTAT_UNPLUGGED && stat == MEDIASTAT_NOTMOUNTED))
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
    if (m_ignoreList.contains(device->getMountPath()) ||
        m_ignoreList.contains(device->getRealDevice())||
        m_ignoreList.contains(device->getDevicePath()) )
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
                 "Paths not in: " + m_ignoreList.join(", "));
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
        auto *me = dynamic_cast<MythMediaEvent *>(event);
        if (me == nullptr)
        {
            LOG(VB_GENERAL, LOG_ALERT,
                     "MediaMonitor::eventFilter() couldn't cast event");
            return true;
        }

        MythMediaDevice *pDev = me->getDevice();
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
                if ((*itr).MythMediaType & (int)pDev->getMediaType() ||
                    pDev->getStatus() == MEDIASTAT_OPEN)
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

QString MediaMonitor::defaultDevice(const QString &dbSetting,
                                    const QString &label,
                                    const char *hardCodedDefault)
{
    QString device = gCoreContext->GetSetting(dbSetting);

    LOG(VB_MEDIA, LOG_DEBUG,
             QString("MediaMonitor::defaultDevice(%1,..,%2) dbSetting='%3'")
                 .arg(dbSetting, hardCodedDefault, device));

    // No settings database defaults? Try to choose one:
    if (device.isEmpty() || device == "default")
    {
        device = hardCodedDefault;

        if (!s_monitor)
            s_monitor = GetMediaMonitor();

        if (s_monitor)
        {
            MythMediaDevice *d = s_monitor->selectDrivePopup(label, false, true);

            if (d == (MythMediaDevice *) -1)    // User cancelled
            {
                device.clear(); // If user has explicitly cancelled return empty string
                d = nullptr;
            }

            if (d && s_monitor->ValidateAndLock(d))
            {
                device = d->getDevicePath();
                s_monitor->Unlock(d);
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
QString MediaMonitor::listDevices(void)
{
    QStringList list;

    for (const auto *dev : qAsConst(m_devices))
    {
        QString devStr;
        QString model = dev->getDeviceModel();
        const QString& path  = dev->getDevicePath();
        const QString& real  = dev->getRealDevice();

        if (path != real)
            devStr += path + "->";
        devStr += real;

        if (model.isEmpty())
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
#elif defined(Q_OS_DARWIN)
        QString def = DEFAULT_CD;
        LOG(VB_MEDIA, LOG_INFO, "Trying 'diskutil eject " + def);
        myth_system("diskutil eject " + def);
#endif
    }
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
