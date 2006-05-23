// -*- Mode: c++ -*-

// Standard C headers
#include <cstdio>

// POSIX headers
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstab.h>

// UNIX System headers
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

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
#include "mythcdrom.h"
#include "mythhdd.h"

#ifndef MNTTYPE_ISO9660
#ifdef linux
#define MNTTYPE_ISO9660 "iso9660"
#elif defined(__FreeBSD__) || defined(CONFIG_DARWIN) || defined(__OpenBSD__)
#define MNTTYPE_ISO9660 "cd9660"
#endif
#endif

#ifndef MNTTYPE_UDF
#define MNTTYPE_UDF "udf"
#endif

#ifndef MNTTYPE_AUTO
#define MNTTYPE_AUTO "auto"
#endif

#ifndef MNTTYPE_SUPERMOUNT
#define MNTTYPE_SUPERMOUNT "supermount"
#endif
#define SUPER_OPT_DEV "dev="

const QString MediaMonitor::kUDEV_FIFO = "/tmp/mythtv_media";
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
        c_monitor = new MediaMonitor(NULL, 500, true);
        c_monitor->CheckFileSystemTable();
        c_monitor->CheckMountable();
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

    int status = selected->getStatus();
    QString dev = selected->getVolumeID();

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
    }
    else
    {
        selected->unlock();

        if (selected->eject() == MEDIAERR_UNSUPPORTED)
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      "eject success",
                                      tr("You may safely remove %1").arg(dev));
        }
        else if (selected->eject() == MEDIAERR_FAILED)
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      "eject fail",
                                      tr("Failed to eject %1").arg(dev));
        }
    }
}

MediaMonitor::MediaMonitor(QObject* par, unsigned long interval, 
                           bool allowEject) 
    : QObject(par), m_Active(false), m_Thread(this, interval),
      m_AllowEject(allowEject), m_fifo(-1)
{
}

MediaMonitor::~MediaMonitor()
{
    if (m_fifo > 0)
    {
        close(m_fifo);
        unlink(kUDEV_FIFO);
    }
}

// Loop through the file system table and add any supported devices.
bool MediaMonitor::CheckFileSystemTable(void)
{
    struct fstab * mep = NULL;
    
    // Attempt to open the file system descriptor entry.
    if (!setfsent()) 
    {
        VERBOSE(VB_IMPORTANT, QString("MediaMonitor") + 
                "MediaMonitor::CheckFileSystemTable - Failed to open " +
                _PATH_FSTAB + " for reading." + ENO);
        return false;
    }
    else 
    {
        // Add all the entries
        while ((mep = getfsent()) != NULL)
            AddDevice(mep);
        
        endfsent();
    }

    if (m_Devices.isEmpty())
        return false;
    
    return true;
}

/** \fn MediaMonitor::CheckMountable(void)
 *  \brief Search /sys/block for valid removable media devices.
 *
 *   This function creates MediaDevice instances for valid removable media
 *   devices found under the /sys/block filesystem in Linux.  CD and DVD
 *   devices are created as MythCDROM instances.  MythHDD instances will be
 *   created for each partition on removable hard disk devices, if they exists.
 *   Otherwise a single MythHDD instance will be created for the entire disc.
 *
 *   NOTE: Floppy disks are ignored.
 */
bool MediaMonitor::CheckMountable(void)
{
#ifdef linux
    mkfifo(kUDEV_FIFO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    m_fifo = open(kUDEV_FIFO, O_RDONLY | O_NONBLOCK);

    QDir sysfs("/sys/block");
    sysfs.setFilter(QDir::Dirs);

    QStringList devices = sysfs.entryList();

    for (QStringList::iterator it = devices.begin(); it != devices.end(); ++it)
    {
        if (*it == "." || *it == "..")
            continue;

        // ignore floppies, too slow
        if ((*it).startsWith("fd"))
            continue;

        sysfs.cd(*it);

        QFile removable(sysfs.absFilePath("removable"));
        if (removable.exists())
        {
            removable.open(IO_ReadOnly);
            int c = removable.getch();
            removable.close();

            if (c == '1')
                FindPartitions(sysfs.absPath(), true);
        }
        sysfs.cdUp();
    }
    return true;
#else // if !linux
    return false;
#endif // !linux
}

/** \fn MediaMonitor::GetDeviceFile(const QString&)
 *  \brief Returns the device special file associated with the /sys/block node.
 *  \param sysfs system filesystem path of removable block device.
 *  \return path to the device special file
 */
QString MediaMonitor::GetDeviceFile(const QString &sysfs)
{
#ifdef linux
    QProcess udevinfo(this);
    udevinfo.addArgument("udevinfo");
    udevinfo.addArgument("-q");
    udevinfo.addArgument("name");
    udevinfo.addArgument("-rp");
    udevinfo.addArgument(sysfs);
    udevinfo.setCommunication(QProcess::Stdout);

    udevinfo.start();

    int status;
    waitpid(udevinfo.processIdentifier(), &status, 0);

    QString ret = udevinfo.readLineStdout();
    if (ret != "device not found in database")
        return ret;
#endif // linux
    (void)sysfs;
    return QString::null;
}

/** \fn MediaMonitor::GetCDROMBlockDevices(void)
 *  \brief Reads the list devices known to be CD or DVD devices.
 *  \return list of CD and DVD device names.
 */
QStringList MediaMonitor::GetCDROMBlockDevices(void)
{
    QStringList l;

#ifdef linux
    QFile file("/proc/sys/dev/cdrom/info");
    if (file.open(IO_ReadOnly))
    {
        QTextStream stream(&file);
        QString line;
        while (!stream.atEnd())
        {
            line = stream.readLine();
            if (line.startsWith("drive name:"))
            {
                QString s = line.section('\t', 2, 2);
                l.append(s);
            }
        }
        file.close();
    }
#endif // linux

    return l;
}

// Given a media deivce add it to our collection.
bool MediaMonitor::AddDevice(MythMediaDevice* pDevice)
{
    if ( ! pDevice )
    {
        VERBOSE(VB_IMPORTANT, "Error - MediaMonitor::AddDevice(null)");
        return false;
    }

    dev_t new_rdev;
    struct stat sb;

    if (stat(pDevice->getDevicePath(), &sb) < 0)
    {
        VERBOSE(VB_IMPORTANT, "MediaMonitor::AddDevice() -- " +
                QString("Failed to stat '%1'")
                .arg(pDevice->getDevicePath()) + ENO);

        return false;
    }
    new_rdev = sb.st_rdev;

    //
    // Check if this is a duplicate of a device we have already added
    //
    QValueList<MythMediaDevice*>::const_iterator itr = m_Devices.begin();
    for (; itr != m_Devices.end(); ++itr)
    {
        if (stat((*itr)->getDevicePath(), &sb) < 0)
        {
            VERBOSE(VB_IMPORTANT, "MediaMonitor::AddDevice() -- " +
                    QString("Failed to stat '%1'")
                    .arg(pDevice->getDevicePath()) + ENO);

            return false;
        }

        if (sb.st_rdev == new_rdev)
        {
            VERBOSE(VB_IMPORTANT, "Mediamonitor: AddDevice() -- " +
                    QString("Not adding '%1', it appears to be a duplicate.")
                    .arg(pDevice->getDevicePath()));

            return false;
        }
    }

    QMutexLocker locker(&m_DevicesLock);

    connect(pDevice, SIGNAL(statusChanged(MediaStatus, MythMediaDevice*)), 
            this, SLOT(mediaStatusChanged(MediaStatus, MythMediaDevice*)));
    m_Devices.push_back( pDevice );
    m_UseCount[pDevice] = 0;

    return true;
}

// Given a fstab entry to a media device determine what type of device it is 
bool MediaMonitor::AddDevice(struct fstab * mep) 
{
    QString devicePath( mep->fs_spec );
    //cout << "AddDevice - " << devicePath << endl;

    MythMediaDevice* pDevice = NULL;
    struct stat sbuf;

    bool is_supermount = false;
    bool is_cdrom = false;

    if (mep == NULL)
       return false;

    if (stat(mep->fs_spec, &sbuf) < 0)
       return false;   

    //  Can it be mounted?  
    if ( ! ( ((strstr(mep->fs_mntops, "owner") && 
        (sbuf.st_mode & S_IRUSR)) || strstr(mep->fs_mntops, "user")) && 
        (strstr(mep->fs_vfstype, MNTTYPE_ISO9660) || 
         strstr(mep->fs_vfstype, MNTTYPE_UDF) || 
         strstr(mep->fs_vfstype, MNTTYPE_AUTO)) ) ) 
    {
       if ( strstr(mep->fs_mntops, MNTTYPE_ISO9660) && 
            strstr(mep->fs_vfstype, MNTTYPE_SUPERMOUNT) ) 
          {
             is_supermount = true;
          }
          else
          {
             return false;
          }
     }

     if (strstr(mep->fs_mntops, MNTTYPE_ISO9660)  || 
         strstr(mep->fs_vfstype, MNTTYPE_ISO9660) || 
         strstr(mep->fs_vfstype, MNTTYPE_UDF)     || 
         strstr(mep->fs_vfstype, MNTTYPE_AUTO)) 
     {
         is_cdrom = true;
         //cout << "Device is a CDROM" << endl;
     }

     if (!is_supermount) 
     {
         if (is_cdrom)
             pDevice = MythCDROM::get(this, QString(mep->fs_spec),
                                     is_supermount, m_AllowEject);
     }
     else 
     {
         char *dev;
         int len = 0;
         dev = strstr(mep->fs_mntops, SUPER_OPT_DEV);
         dev += sizeof(SUPER_OPT_DEV)-1;
         while (dev[len] != ',' && dev[len] != ' ' && dev[len] != 0)
             len++;

         if (dev[len] != 0) 
         {
             char devstr[256];
             strncpy(devstr, dev, len);
             devstr[len] = 0;
             if (is_cdrom)
                 pDevice = MythCDROM::get(this, QString(devstr),
                                is_supermount, m_AllowEject);
         }
         else
             return false;   
     }
        
     if (pDevice) 
     {
         pDevice->setMountPath(mep->fs_file);
         VERBOSE(VB_IMPORTANT, QString("Mediamonitor: Adding %1")
                         .arg(pDevice->getDevicePath()));
         if (pDevice->testMedia() == MEDIAERR_OK) 
         {
             if (AddDevice(pDevice))
                return true;
         }
         delete pDevice;
      }

     return false;
}

// Given a path to a media device determine what type of device it is and 
// add it to our collection.
bool MediaMonitor::AddDevice(const char* devPath ) 
{
    QString devicePath( devPath );
    //cout << "AddDevice - " << devicePath << endl;

    struct fstab * mep = NULL;
    char lpath[PATH_MAX];

    // Resolve the simlink for the device.
    int len = readlink(devicePath, lpath, PATH_MAX);
    if (len > 0 && len < PATH_MAX)
        lpath[len] = 0;

    // Attempt to open the file system descriptor entry.
    if (!setfsent()) 
    {
        perror("setfsent");
        cerr << "MediaMonitor::AddDevice - Failed to open "
                _PATH_FSTAB
                " for reading." << endl;
        return false;
    }
    else 
    {
        // Loop over the file system descriptor entry.
        while ((mep = getfsent()) != NULL)  
        {
//             cout << "***************************************************" << endl;
//              cout << "devicePath == " << devicePath << endl;
//              cout << "mep->fs_spec == " << mep->fs_spec << endl;
//              cout << "lpath == " << lpath << endl; 
//              cout << "strcmp(devicePath, mep->fs_spec) == " << strcmp(devicePath, mep->fs_spec) << endl;
//              cout << "len ==" << len << endl;
//              cout << "strcmp(lpath, mep->fs_spec)==" << strcmp(lpath, mep->fs_spec) << endl;
//              cout <<endl << endl;

            // Check to see if this is the same as our passed in device. 
            if ((strcmp(devicePath, mep->fs_spec) != 0) && 
                 (len && (strcmp(lpath, mep->fs_spec) != 0)))
                continue;

        }

        endfsent();
    }

    if (mep) 
       return AddDevice(mep); 

    return false;
}

/** \fn MediaMonitor::FindPartitions(const QString&, bool)
 *  \brief Creates MythMedia instances for sysfs removable media devices.
 *
 *   Block devices are represented as directories in sysfs with directories
 *   for each partition underneath the parent device directory.
 *
 *   This function recursively calls itself to find all partitions on a block
 *   device and creates a MythHDD instance for each partition found.  If no
 *   partitions are found and the device is a CD or DVD device a MythCDROM
 *   instance is created.  Otherwise a MythHDD instance is created for the
 *   entire block device.
 *
 *  \param dev path to sysfs block device.
 *  \param checkPartitions check for partitions on block device.
 *  \return true if MythMedia instances are created.
 */
bool MediaMonitor::FindPartitions(const QString &dev, bool checkPartitions)
{
    MythMediaDevice* pDevice = NULL;

    if (checkPartitions)
    {
        // check for partitions
        QDir sysfs(dev);
        sysfs.setFilter(QDir::Dirs);

        bool found_partitions = false;
        QStringList parts = sysfs.entryList();
        for (QStringList::iterator pit = parts.begin();
             pit != parts.end(); pit++)
        {
            if (*pit == "." || *pit == "..")
                continue;

            found_partitions |= FindPartitions(sysfs.absFilePath(*pit), false);
        }

        // no partitions on block device, use main device
        if (!found_partitions)
            found_partitions |= FindPartitions(sysfs.absPath(), false);

        return found_partitions;
    }

    QStringList cdroms = GetCDROMBlockDevices();

    if (cdroms.contains(dev.section('/', -1)))
    {
        // found cdrom device
        QString device_file = GetDeviceFile(dev);
        if (!device_file.isNull())
        {
            pDevice = MythCDROM::get(this, device_file, false, m_AllowEject);

            if (AddDevice(pDevice))
                return true;
        }
    }
    else
    {
        // found block or partition device
        QString device_file = GetDeviceFile(dev);
        if (!device_file.isNull())
        {
            pDevice = MythHDD::Get(this, device_file, false, false);
            if (AddDevice(pDevice))
                return true;
        }
    }

    if (pDevice)
        delete pDevice;

    return false;
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

/** \fn MediaMonitor::CheckDeviceNotifications()
 *  \brief Checks the named pipe, kUDEV_FIFO, for
 *         hotplug events from the udev system.
 *   NOTE: Currently only Linux w/udev 0.71+ provides these events.
 */
void MediaMonitor::CheckDeviceNotifications(void)
{
    char buffer[256];
    QString qBuffer = "";

    if (!m_fifo)
        return;

    int size = read(m_fifo, buffer, 255);
    while (size > 0)
    {
        // append buffer to QString
        buffer[size] = '\0';
        qBuffer.append(buffer);
        size = read(m_fifo, buffer, 255);
    }
    const QStringList list = QStringList::split('\n', qBuffer);

    QStringList::const_iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        if ((*it).startsWith("add"))
        {
            QString dev = (*it).section(' ', 1, 1);
            
            // check if removeable
            QFile removable(dev + "/removable");
            if (removable.exists())
            {
                removable.open(IO_ReadOnly);
                int c = removable.getch();
                removable.close();

                if (c == '1')
                    FindPartitions((*it).section(' ', 1, 1), true);
            }
        }
        else if ((*it).startsWith("remove"))
        {
            RemoveDevice((*it).section(' ', 2, 2));
        }
    }
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

    m_Active = true;
    m_Thread.start();
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
    m_Thread.wait();
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
