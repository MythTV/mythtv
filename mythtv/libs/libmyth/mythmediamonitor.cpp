
// Standard C headers
#include <cstdio>

// C++ headers
#include <iostream>
#include <typeinfo>
using namespace std;

// Qt headers
#include <qapplication.h>
#include <qprocess.h>
#include <qdir.h>
#include <qfile.h>

// MythTV headers
#include "mythmediamonitor.h"
#include "mythcontext.h"
#include "mythdialogs.h"
#include "mythconfig.h"

#ifdef USING_DARWIN_DA
#include "mediamonitor-darwin.h" 
#endif
#if defined(CONFIG_CYGWIN) || defined(_WIN32)
#include "mediamonitor-windows.h" 
#else
#include "mediamonitor-unix.h" 
#endif

MediaMonitor *MediaMonitor::c_monitor = NULL;

// MonitorThread
MonitorThread::MonitorThread(MediaMonitor* pMon, unsigned long interval) 
             : QThread()
{
    m_Monitor = pMon;
    m_Interval = interval;
}

// Nice and simple, as long as our monitor is valid and active, 
// loop and check it's devices.
void MonitorThread::run(void)
{
    while (m_Monitor && m_Monitor->IsActive())        
    {
        m_Monitor->CheckDevices();
        msleep(m_Interval);
    }
}

////////////////////////////////////////////////////////////////////////
// MediaMonitor


MediaMonitor* MediaMonitor::GetMediaMonitor(void)
{
    if (c_monitor)
        return c_monitor;

#ifdef USING_DARWIN_DA
    c_monitor = new MediaMonitorDarwin(NULL, 500, true); 
#else
  #if defined(CONFIG_CYGWIN) || defined(_WIN32)
    c_monitor = new MediaMonitorWindows(NULL, 500, true); 
  #else
    c_monitor = new MediaMonitorUnix(NULL, 500, true);
  #endif
#endif

    return c_monitor;
}

void MediaMonitor::SetCDSpeed(const char *device, int speed)
{
    MediaMonitor *mon = GetMediaMonitor();
    if (mon != NULL)
    {
        MythMediaDevice *pMedia = mon->GetMedia(device);
        if (pMedia && mon->ValidateAndLock(pMedia))
        {
            pMedia->setSpeed(speed);
            mon->Unlock(pMedia);
        }
    }
}

// When ejecting one of multiple devices, present a nice name to the user
static const QString DevName(MythMediaDevice *d)
{
    QString str = d->getVolumeID();  // First choice, the name of the media 

    if (str == "")
    {
        str = d->getDeviceModel();   // otherwise, the drive manufacturer/model

        if (str)                     // and/or the device node
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
 * \brief Popup a dialog box for the user to select one drive.
 *  
 * Has to iterate through all devices to check if any are suitable,
 * prevent drawing a list if there is only one drive, et cetera
 */ 
MythMediaDevice * MediaMonitor::selectDrivePopup(const QString label,
                                                 bool          showMounted)
{       
    QValueList <MythMediaDevice *>           drives;
    QValueList <MythMediaDevice *>::iterator it = m_Devices.begin();
    QMutexLocker                             locker(&m_DevicesLock);

    for (it = m_Devices.begin(); it != m_Devices.end(); ++it)
    {
        // By default, we only list CD/DVD devices.
        // Caller can also request mounted drives to be listed (e.g. USB flash)

        if (QString(typeid(**it).name()).contains("MythCDROM") ||
               (showMounted && (*it)->isMounted()))
            drives.append(*it);
    }

    if (drives.count() == 0)
    {
        VERBOSE(VB_MEDIA, "MediaMonitor::selectDrivePopup("
                          + label + ") - No suitable devices");
        return NULL;
    }

    if (drives.count() == 1)
    {
        VERBOSE(VB_MEDIA, "MediaMonitor::selectDrivePopup("
                          + label + ") - One suitable device");
        return drives.front();
    }

    QStringList buttonmsgs;
    for (it = drives.begin(); it != drives.end(); ++it)
        buttonmsgs += DevName(*it);
    buttonmsgs += tr("Cancel");
    const DialogCode cancelbtn = (DialogCode)
        (((int)kDialogCodeButton0) + buttonmsgs.size() - 1);

    DialogCode ret = MythPopupBox::ShowButtonPopup(
        gContext->GetMainWindow(), "select drive", label,
        buttonmsgs, cancelbtn);

    // If the user cancelled, return a special value
    if ((kDialogCodeRejected == ret) || (cancelbtn == ret))
        return (MythMediaDevice *)-1;

    uint idx = MythDialog::CalcItemIndex(ret);
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
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  "nothing to eject ",
                                  tr("No devices to eject"));
        return;
    }

    QString  dev = DevName(selected);

    if (selected->getStatus() == MEDIASTAT_OPEN)
    {
        VERBOSE(VB_MEDIA,
                QString("Disk %1's tray is OPEN. Closing tray").arg(dev));

        if (selected->eject(false) != MEDIAERR_OK)
        {
            QString msg = "Unable to open or close the empty drive %1.\n\n";
            msg += "You may have to use the eject button under its tray.";
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      "eject close-tray fail",
                                      tr(msg).arg(dev));
        }

        return;
    }

    if (selected->isMounted(true))
    {
        VERBOSE(VB_MEDIA, QString("Disk %1 is mounted? Unmounting").arg(dev));
        selected->unmount();

        if (selected->isMounted(true))
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      "eject unmount fail",
                                      tr("Failed to unmount %1").arg(dev));
            return;
        }
    }

    VERBOSE(VB_MEDIA, QString("Unlocking disk %1, then eject()ing").arg(dev));
    selected->unlock();

    MediaError err = selected->eject();

    if (err == MEDIAERR_UNSUPPORTED)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), "eject success",
                                  tr("You may safely remove %1").arg(dev));
    }
    else if (err == MEDIAERR_FAILED)
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), "eject fail",
                                  tr("Failed to eject %1").arg(dev));
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
    m_StartThread = gContext->GetNumSetting("MonitorDrives");

    // or not send status changed events:
    m_SendEvent = gContext->GetNumSetting("MediaChangeEvents");

    // User can specify that some devices are not monitored
    QString ignore = gContext->GetSetting("IgnoreDevices", "");

    if (ignore.length())
        m_IgnoreList = QStringList::split(',', ignore);
    else
        m_IgnoreList = QStringList::QStringList();  // Force empty list

    if (m_StartThread)
        VERBOSE(VB_MEDIA, "Creating MediaMonitor, SendEvents="
                          + (m_SendEvent?QString("true"):QString("false")));
    else
#ifdef USING_DARWIN_DA
        VERBOSE(VB_MEDIA, "MediaMonitor is disabled. Eject will not work");
#else
        VERBOSE(VB_MEDIA,
                "Creating inactive MediaMonitor and static device list");
#endif
    VERBOSE(VB_MEDIA, "IgnoreDevices=" + ignore);

    // If any of IgnoreDevices are symlinks, also add the real device
    QStringList::Iterator dev;
    for (dev = m_IgnoreList.begin(); dev != m_IgnoreList.end(); ++dev)
    {
        QFileInfo *fi = new QFileInfo(*dev);

        if (fi && fi->isSymLink())
        {
            QString target = fi->readLink();

            if (m_IgnoreList.grep(target).isEmpty())
            {
                VERBOSE(VB_MEDIA, "Also ignoring " + target +
                                  " (symlinked from " + *dev + ").");
                m_IgnoreList += target;
            }
        }
    }
}

MediaMonitor::~MediaMonitor()
{
    delete m_Thread;
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

    QValueList<MythMediaDevice*>::iterator it;
    for (it = m_Devices.begin(); it != m_Devices.end(); it++)
    {
        if ((*it)->getDevicePath() == dev)
        {
            if (m_UseCount[*it] == 0)
            {
                (*it)->deleteLater();
                m_Devices.remove(it);
                m_UseCount.remove(*it);
            }
            else
            {
                // Other threads are still using this device
                // postpone actual delete until they finish.
                disconnect(*it);
                m_RemovedDevices.append(*it);
                m_Devices.remove(it);
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

    QValueList<MythMediaDevice*>::iterator itr = m_Devices.begin();
    MythMediaDevice* pDev;
    while (itr != m_Devices.end()) 
    {
        pDev = *itr;
        if (pDev)
            pDev->checkMedia();
        itr++;
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

    VERBOSE(VB_MEDIA, "Starting MediaMonitor");
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

    VERBOSE(VB_MEDIA, "Stopping MediaMonitor");
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
 *  \sa Unlock(MythMediaDevice *pMedia), GetMedias(MediaType mediatype)
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
 *  \sa ValidateAndLock(MythMediaDevice *pMedia), GetMedias(MediaType mediatype)
 */
void MediaMonitor::Unlock(MythMediaDevice *pMedia)
{
    QMutexLocker locker(&m_DevicesLock);

    if (!m_UseCount.contains(pMedia))
        return;

    m_UseCount[pMedia]--;

    if (m_UseCount[pMedia] == 0 && m_RemovedDevices.contains(pMedia))
    {
        m_RemovedDevices.remove(pMedia);
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

    QValueList<MythMediaDevice*>::iterator it = m_Devices.begin();
    for (;it != m_Devices.end(); it++)
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
    }

    return mountPath;
}

/** \fn MediaMonitor::GetMedias(MediaType mediatype)
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
QValueList<MythMediaDevice*> MediaMonitor::GetMedias(MediaType mediatype)
{
    QMutexLocker locker(&m_DevicesLock);

    QValueList<MythMediaDevice*> medias;

    QValueList<MythMediaDevice*>::iterator it = m_Devices.begin();
    for (;it != m_Devices.end(); it++)
    {
        if (((*it)->getMediaType() == mediatype) &&
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
    VERBOSE(VB_IMPORTANT, QString("MonitorRegisterExtensions(0x%1, %2)")
            .arg(mediatype, 0, 16).arg(extensions));

    QValueList<MythMediaDevice*>::iterator it = m_Devices.begin();
    for (; it != m_Devices.end(); ++it)
    {
        if (*it)
            (*it)->RegisterMediaExtensions(mediatype, extensions);
    }
}

// Signal handler.
void MediaMonitor::mediaStatusChanged(MediaStatus oldStatus, 
                                      MythMediaDevice* pMedia)
{
    // If we're not active then ignore signal.
    if (!m_Active)
        return;

    MediaStatus  stat = pMedia->getStatus();
    QString      msg  = QString(" (%1, %2 -> %3)")
                        .arg(pMedia->MediaTypeString())
                        .arg(MythMediaDevice::MediaStatusStrings[oldStatus])
                        .arg(MythMediaDevice::MediaStatusStrings[stat]);

    // This gets called from outside the main thread so we need
    // to post an event back to the main thread.
    // We now send events for all non-error statuses, so plugins get ejects
    if (m_SendEvent && stat != MEDIASTAT_ERROR && stat != MEDIASTAT_UNKNOWN)
    {
        VERBOSE(VB_MEDIA, "Posting MediaEvent" + msg);

        // sendEvent() is needed here - it waits for the event to be used.
        // postEvent() would result in pDevice's media type changing
        // ... before the plugin's event chain would process it.
        // Another way would be to send an exact copy of pDevice instead.
        QApplication::sendEvent((QObject*)gContext->GetMainWindow(),
                                new MediaEvent(stat, pMedia));
    }
    else
        VERBOSE(VB_MEDIA, "Media status changed, but not sending event" + msg);


    if (stat == MEDIASTAT_OPEN || stat == MEDIASTAT_NODISK
                               || stat == MEDIASTAT_UNPLUGGED)
    {
        pMedia->clearData();
    }
}

/**
 * Check user preferences to see if this device should be monitored
 */
bool MediaMonitor::shouldIgnore(MythMediaDevice* device)
{
    if (m_IgnoreList.contains(device->getMountPath()) ||
        m_IgnoreList.contains(device->getRealDevice())||
        m_IgnoreList.contains(device->getDevicePath()) )
    {
        VERBOSE(VB_MEDIA, "Ignoring device: " + device->getDevicePath());
        return true;
    }
#if 0
    else
    {
        VERBOSE(VB_MEDIA, "Not ignoring: " + device->getDevicePath()
                          + " / " + device->getMountPath());
        VERBOSE(VB_MEDIA, "Paths not in: " + m_IgnoreList.join(", "));
    }
#endif

    return false;
}

/*
 * These methods return the user's preferred devices for playing and burning
 * CDs and DVDs. Traditionally we had a database setting to remember this,
 * but that is a bit wasteful when most users only have one drive.
 *
 * To make it a bit more beginner friendly, if no database default exists,
 * or if it contins "default", the code tries to guess the correct drive,
 * or put a dialog box up if there are several valid options.
 * If, still, nothing is suitable, a caller hard-coded default is used.
 *
 * Ideally, these would return a MythMediaDevice * instead of a QString
 */

QString MediaMonitor::defaultDevice(QString dbSetting,
                                    QString label, char *hardCodedDefault)
{
    QString device = gContext->GetSetting(dbSetting);

    // No settings database defaults? Try to choose one:
    if (device.isEmpty() || device == "default")
    {
        device = hardCodedDefault;

        if (!c_monitor)
            c_monitor = GetMediaMonitor();

        if (c_monitor)
        {
            MythMediaDevice *d = c_monitor->selectDrivePopup(label);

            if (d == (MythMediaDevice *) -1)    // User cancelled
                d = NULL;

            if (d)
                device = d->getDevicePath();
        }
    }

#if 0
    // A hack to do VERBOSE in a static method:
    extern unsigned int print_verbose_messages;
    if (print_verbose_messages & 0x00800000)  // VB_MEDIA
    {
        QDateTime dtmp = QDateTime::currentDateTime();
        QString dtime = dtmp.toString("yyyy-MM-dd hh:mm:ss.zzz");
        cout << dtime << " MediaMonitor::defaultDevice('" << dbSetting.ascii()
             << "', '" << label.ascii() << "'') returning '"
             << device.ascii() << "'" << endl;
    }
#endif

    return device;
}

/**
 * CDDevice, user-selected drive, or /dev/cdrom
 */
QString MediaMonitor::defaultCDdevice()
{
    return defaultDevice("CDDevice", tr("Select a CD drive"), "/dev/cdrom");
}

/**
 * VCDDeviceLocation, user-selected drive, or /dev/cdrom
 */
QString MediaMonitor::defaultVCDdevice()
{
    return defaultDevice("VCDDeviceLocation",
                         tr("Select a VCD drive"), "/dev/cdrom");
}

/**
 * DVDDeviceLocation, user-selected drive, or /dev/dvd
 */
QString MediaMonitor::defaultDVDdevice()
{
    return defaultDevice("DVDDeviceLocation",
                         tr("Select a DVD drive"), "/dev/dvd");
}

/**
 * \brief MythArchiveDVDLocation, user-selected drive, or /dev/dvd
 *
 * This should also look for drives with blanks or RWs in them,
 * but Nigel hasn't worked out how to do this tidily (yet).
 */
QString MediaMonitor::defaultWriter()
{
    QString device = defaultDevice("MythArchiveDVDLocation",
                                   tr("Select a DVD writer"), "/dev/dvd");

    return device;
}


/**
 * \brief A string summarising the current devices, for debugging
 */
const QString MediaMonitor::listDevices(void)
{
    QValueList<MythMediaDevice*>::const_iterator dev;
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
