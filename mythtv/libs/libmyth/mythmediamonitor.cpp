
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
    MythMediaDevice *selected = NULL;

    QMutexLocker locker(&m_DevicesLock);

    if (m_Devices.count() == 1)
    {
        if (m_Devices.first()->getAllowEject())
            selected = m_Devices.first();
    }
    else if (m_Devices.count() > 1)
    {
        MythPopupBox ejectbox(gContext->GetMainWindow(), "eject media");

        ejectbox.addLabel(tr("Select removable media to eject"));

        QValueList <MythMediaDevice *> shownDevices;
        QValueList <MythMediaDevice *>::iterator it = m_Devices.begin();
        while (it != m_Devices.end())
        {
            // if the device is ejectable (ie a CD or DVD device)
            // or if it is mounted (ie a USB memory stick)
            // then add it to the list of choices
            if ((*it)->getAllowEject() || (*it)->isMounted(true))
            {
                shownDevices.append(*it);

                if ((*it)->getVolumeID() != "")
                    ejectbox.addButton((*it)->getVolumeID());
                else
                    ejectbox.addButton((*it)->getDevicePath());
            }

            it++;
        }

        ejectbox.addButton(tr("Cancel"))->setFocus();

        int ret = ejectbox.ExecPopup();

        if ((uint)ret < shownDevices.count())
            selected = shownDevices[ret];
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  "nothing to eject ",
                                  tr("No devices to eject"));
    }

    if (!selected)
        return;

    bool doEject = false;
    int   status = selected->getStatus();
    QString  dev = selected->getVolumeID();

    if (dev == "")
        dev = selected->getDevicePath();

    if (MEDIASTAT_OPEN == status)
    {
        selected->eject(false);
    }
    else if (MEDIASTAT_MOUNTED == status)
    {
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
