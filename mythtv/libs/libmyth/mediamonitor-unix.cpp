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
#include "mediamonitor-unix.h"
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

const QString MediaMonitorUnix::kUDEV_FIFO = "/tmp/mythtv_media";

////////////////////////////////////////////////////////////////////////
// MediaMonitor


MediaMonitorUnix::MediaMonitorUnix(QObject* par,
                                   unsigned long interval, bool allowEject)
                : MediaMonitor(par, interval, allowEject)
{
    CheckFileSystemTable();
    CheckMountable();
}


MediaMonitorUnix::~MediaMonitorUnix()
{
    if (m_fifo > 0)
    {
        close(m_fifo);
        unlink(kUDEV_FIFO);
    }
}


// Loop through the file system table and add any supported devices.
bool MediaMonitorUnix::CheckFileSystemTable(void)
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

/**
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
bool MediaMonitorUnix::CheckMountable(void)
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

/**
 *  \brief Returns the device special file associated with the /sys/block node.
 *  \param sysfs system filesystem path of removable block device.
 *  \return path to the device special file
 */
QString MediaMonitorUnix::GetDeviceFile(const QString &sysfs)
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

/*
 *  \brief Reads the list devices known to be CD or DVD devices.
 *  \return list of CD and DVD device names.
 */
QStringList MediaMonitorUnix::GetCDROMBlockDevices(void)
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
bool MediaMonitorUnix::AddDevice(MythMediaDevice* pDevice)
{
    if ( ! pDevice )
    {
        VERBOSE(VB_IMPORTANT, "Error - MediaMonitorUnix::AddDevice(null)");
        return false;
    }

    dev_t new_rdev;
    struct stat sb;

    if (stat(pDevice->getDevicePath(), &sb) < 0)
    {
        VERBOSE(VB_IMPORTANT, "MediaMonitorUnix::AddDevice() -- " +
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
            VERBOSE(VB_IMPORTANT, "MediaMonitorUnix::AddDevice() -- " +
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
bool MediaMonitorUnix::AddDevice(struct fstab * mep) 
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
bool MediaMonitorUnix::AddDevice(const char* devPath ) 
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
        cerr << "MediaMonitorUnix::AddDevice - Failed to open "
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

/**
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
bool MediaMonitorUnix::FindPartitions(const QString &dev, bool checkPartitions)
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

/**
 *  \brief Checks the named pipe, kUDEV_FIFO, for
 *         hotplug events from the udev system.
 *   NOTE: Currently only Linux w/udev 0.71+ provides these events.
 */
void MediaMonitorUnix::CheckDeviceNotifications(void)
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
