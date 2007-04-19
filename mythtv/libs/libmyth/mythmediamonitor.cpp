
// Standard C headers
#include <cstdio>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <qapplication.h>
#include <qprocess.h>
#include <qdir.h>
#include <qfile.h>

// MythTV headers
#include "mythcdrom.h"
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

    if (gContext->GetNumSetting("MonitorDrives"))
    {
#ifdef USING_DARWIN_DA
        c_monitor = new MediaMonitorDarwin(NULL, 500, true); 
#else
  #if defined(CONFIG_CYGWIN) || defined(_WIN32)
        c_monitor = new MediaMonitorWindows(NULL, 500, true); 
  #else
        c_monitor = new MediaMonitorUnix(NULL, 500, true);
  #endif
#endif
    }

    return c_monitor;
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
        // Caller can also request mounted drives to be listed:

        if (typeid(**it) == typeid(MythCDROM) ||
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

    MythPopupBox box(gContext->GetMainWindow(), "select drive");
    box.addLabel(label);
    for (it = drives.begin(); it != drives.end(); ++it)
        box.addButton(DevName(*it));

    box.addButton(tr("Cancel"))->setFocus();

    int ret = box.ExecPopup();

    // If the user cancelled, return a special value
    if (ret < 0)
        return (MythMediaDevice *)-1;

    if ((uint)ret < drives.count())
        return drives[ret];

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


    selected = selectDrivePopup(tr("Select removable media to eject"), true);

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

    bool doEject = false;
    int   status = selected->getStatus();
    QString  dev = DevName(selected);

    if (MEDIASTAT_OPEN == status)
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
    }
    else if (MEDIASTAT_MOUNTED == status)
    {
        VERBOSE(VB_MEDIA, QString("Disk %1 is mounted? Unmounting").arg(dev));
        selected->unmount();

        if (selected->isMounted(true))
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      "eject unmount fail",
                                      tr("Failed to unmount %1").arg(dev));
        }
        else
            doEject = true;
    }
    else
        doEject = true;

    if (doEject)
    {
        VERBOSE(VB_MEDIA,
                QString("Unlocking disk %1, then eject()ing").arg(dev));
        selected->unlock();

        MediaError err = selected->eject();

        if (err == MEDIAERR_UNSUPPORTED)
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      "eject success",
                                      tr("You may safely remove %1").arg(dev));
        }
        else if (err == MEDIAERR_FAILED)
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      "eject fail",
                                      tr("Failed to eject %1").arg(dev));
        }
    }
}

MediaMonitor::MediaMonitor(QObject* par, unsigned long interval, 
                           bool allowEject) 
    : QObject(par), m_Active(false), m_Thread(NULL),
      m_MonitorPollingInterval(interval), m_AllowEject(allowEject)
{
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
                delete *it;
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

    if (!m_Thread)
        m_Thread = new MonitorThread(this, m_MonitorPollingInterval);

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
        delete pMedia;
    }
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
    
    VERBOSE(VB_IMPORTANT, QString("Media status changed...  New status is: %1 "
                                  "old status was %2")
            .arg(MythMediaDevice::MediaStatusStrings[pMedia->getStatus()])
            .arg(MythMediaDevice::MediaStatusStrings[oldStatus]));

    // This gets called from outside the main thread so we need
    // to post an event back to the main thread.
    // We're only going to pass up events for useable media...
    if (pMedia->getStatus() == MEDIASTAT_USEABLE || 
        pMedia->getStatus() == MEDIASTAT_MOUNTED) 
    {
        VERBOSE(VB_IMPORTANT, "Posting MediaEvent");
        QApplication::postEvent((QObject*)gContext->GetMainWindow(), 
                                new MediaEvent(oldStatus, pMedia));
    }
    else if (pMedia->getStatus() == MEDIASTAT_OPEN ||
             pMedia->getStatus() == MEDIASTAT_UNPLUGGED)
    {
        pMedia->clearData();
    }
}

/*
 * These methods return the user's preferred devices for playing and burning
 * CDs and DVDs. Traditionally we had a database setting to remember this,
 * but that is a bit wasteful when most users only have one drive.
 *
 * To make it a bit more beginner friendly, if no database default exists,
 * or if it contins "default", the code tries to guess the correct drive,
 * or put a dialog box up if there are several valid options.
 *
 * Ideally, these would return a MythMediaDevice * instead of a QString
 */

QString MediaMonitor::defaultDevice(QString dbSetting, QString label)
{
    QString device = gContext->GetSetting(dbSetting);

    // No settings database defaults. Try to choose one:
    if (device.isEmpty() || device == "default")
    {
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

    return device;
}

/**
 * CDDevice, user-selected drive, or /dev/cdrom
 */
QString MediaMonitor::defaultCDdevice()
{
    QString device = defaultDevice("CDDevice", tr("Select a CD drive"));
    if (device.length())
        return device;

    // Last resort:
    return "/dev/cdrom";
}

/**
 * VCDDeviceLocation, user-selected drive, or /dev/cdrom
 */
QString MediaMonitor::defaultVCDdevice()
{
    QString device = defaultDevice("VCDDeviceLocation",
                                   tr("Select a VCD drive"));
    if (device.length())
        return device;

    // Last resort:
    return "/dev/cdrom";
}

/**
 * DVDDeviceLocation, user-selected drive, or /dev/dvd
 */
QString MediaMonitor::defaultDVDdevice()
{
    QString device = defaultDevice("DVDDeviceLocation",
                                   tr("Select a DVD drive"));
    if (device.length())
        return device;

    // Last resort:
    return "/dev/dvd";
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
                                   tr("Select a DVD writer"));
    if (device.length())
        return device;

    // Last resort:
    return "/dev/dvd";
}
