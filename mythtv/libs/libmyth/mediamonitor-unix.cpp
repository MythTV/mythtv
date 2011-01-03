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
#include <sys/param.h>

// C++ headers
#include <iostream>

using namespace std;

// Qt headers
#include <QList>
#include <QTextStream>
#include <QDir>
#include <QFile>

// MythTV headers
#include "mythmediamonitor.h"
#include "mediamonitor-unix.h"
#include "mythdialogs.h"
#include "mythconfig.h"
#include "mythcdrom.h"
#include "mythhdd.h"
#include "mythverbose.h"
#include "mythsystem.h"
#include "exitcodes.h"

#if HAVE_LIBUDEV
extern "C" {
    #include <libudev.h>
}
#endif


#ifndef MNTTYPE_ISO9660
#ifdef linux
#define MNTTYPE_ISO9660 "iso9660"
#elif defined(__FreeBSD__) || CONFIG_DARWIN || defined(__OpenBSD__)
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

const char * MediaMonitorUnix::kUDEV_FIFO = "/tmp/mythtv_media";


// Some helpers for debugging:

static const QString LOC = QString("MMUnix:");

static void fstabError(const QString &methodName)
{
    VERBOSE(VB_IMPORTANT, LOC + methodName + " Error: failed to open "
                          + _PATH_FSTAB + " for reading, " + ENO);
}

static void statError(const QString &methodName, const QString devPath)
{
    VERBOSE(VB_IMPORTANT, LOC + methodName + " Error: failed to stat "
                          + devPath + ", " + ENO);
}

////////////////////////////////////////////////////////////////////////
// MediaMonitor


MediaMonitorUnix::MediaMonitorUnix(QObject* par,
                                   unsigned long interval, bool allowEject)
                : MediaMonitor(par, interval, allowEject)
{
    CheckFileSystemTable();
    CheckMountable();

    VERBOSE(VB_MEDIA, "Initial device list...\n" + listDevices());
}


void MediaMonitorUnix::deleteLater(void)
{
    if (m_fifo >= 0)
    {
        close(m_fifo);
        m_fifo = -1;
        unlink(kUDEV_FIFO);
    }
    MediaMonitor::deleteLater();
}


// Loop through the file system table and add any supported devices.
bool MediaMonitorUnix::CheckFileSystemTable(void)
{
    struct fstab * mep = NULL;

    // Attempt to open the file system descriptor entry.
    if (!setfsent())
    {
        fstabError(":CheckFileSystemTable()");
        return false;
    }

    // Add all the entries
    while ((mep = getfsent()) != NULL)
        AddDevice(mep);

    endfsent();

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
 *   created for each partition on removable hard disk devices, if they exist.
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
        QString path = sysfs.absolutePath();
        if (CheckRemovable(path))
            FindPartitions(path, true);
        sysfs.cdUp();
    }
    return true;
#else // if !linux
    return false;
#endif // !linux
}

/**
 * \brief  Is /sys/block/dev a removable device?
 */
bool MediaMonitorUnix::CheckRemovable(const QString &dev)
{
#ifdef linux
        QString removablePath = dev + "/removable";
        QFile   removable(removablePath);
        if (removable.exists() && removable.open(QIODevice::ReadOnly))
        {
            char    c   = 0;
            QString msg = LOC + ":CheckRemovable(" + dev + ")/removable ";
            bool    ok  = removable.getChar(&c);
            removable.close();

            if (ok)
            {
                VERBOSE(VB_MEDIA+VB_EXTRA, msg + c);
                if (c == '1')
                    return true;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, msg + "failed");
            }
        }
        return false;
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
    QString msg = LOC + ":GetDeviceFile(" + sysfs + ")";
    QString ret = sysfs;

    // In case of error, a working default?  (device names usually match)
    ret.replace(QRegExp(".*/"), "/dev/");

#ifdef linux
  #if HAVE_LIBUDEV
    // Use libudev to determine the name
    ret = "";
    struct udev *udev = udev_new();
    if (udev != NULL)
    {
        struct udev_device *device =
            udev_device_new_from_syspath(udev, sysfs.toAscii().constData());
        if (device != NULL)
        {
            const char *name = udev_device_get_devnode(device);

            if (name != NULL)
                ret = tr(name);

            udev_device_unref(device);
        }
        udev_unref(udev);
    }
  #else   // HAVE_LIBUDEV
    // Use udevadm info to determine the name
    QStringList  args;
    args << "info" << "-q"  << "name"
         << "-rp" << sysfs;

    uint flags = kMSStdOut | kMSBuffered;
    if( VERBOSE_LEVEL_CHECK(VB_MEDIA|VB_EXTRA) )
        flags |= kMSStdErr;

    MythSystem *udevinfo = new MythSystem("udevinfo", args, flags);
    udevinfo->Run(4);
    if( udevinfo->Wait() != GENERIC_EXIT_OK )
    {
        delete udevinfo;
        return ret;
    }

    if (VERBOSE_LEVEL_CHECK(VB_MEDIA|VB_EXTRA))
    {
        QTextStream estream(udevinfo->ReadAllErr());
        while( !estream.atEnd() )
            VERBOSE(VB_MEDIA+VB_EXTRA,
                    msg + " - udevadm info error...\n" + estream.readLine());
    }

    QTextStream ostream(udevinfo->ReadAll());
    ret = ostream.readLine();
    if( ret.startsWith("device not found in database") )
    {
        delete udevinfo;
        return ret;
    }

    delete udevinfo;
  #endif // HAVE_LIBUDEV
#endif // linux

    VERBOSE(VB_MEDIA, msg + "->'" + ret + "'");
    return ret;
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
    if (file.open(QIODevice::ReadOnly))
    {
        QString     line;
        QTextStream stream(&file);
        do
        {
            line = stream.readLine();
            if (line.startsWith("drive name:"))
            {
                l = line.split('\t', QString::SkipEmptyParts);
                l.pop_front();   // Remove 'drive name:' field
                break;           // file should only contain one drive table?
            }
        }
        while (!stream.atEnd());
        file.close();
    }
#endif // linux

    VERBOSE(VB_MEDIA, LOC + ":GetCDROMBlockDevices()->'" + l.join(", ") + "'");
    return l;
}

static void LookupModel(MythMediaDevice* device)
{
    QString   desc;
    QString   devname = device->getRealDevice();


    // Given something like /dev/hda1, extract hda1
    devname = devname.mid(5,5);


#ifdef linux
    if (devname.startsWith("hd"))  // IDE drive
    {
        QFile  file("/proc/ide/" + devname.left(3) + "/model");
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream stream(&file);

            desc.append(stream.readLine());
            file.close();
        }
    }

    if (devname.startsWith("scd"))     // scd0 doesn't appear in /sys/block,
        devname.replace("scd", "sr");  // use sr0 instead

    if (devname.startsWith("sd")       // SATA/USB/FireWire
        || devname.startsWith("sr"))   // SCSI CD-ROM?
    {
        QString path = devname.prepend("/sys/block/");
        path.append("/device/");

        QFile  file(path + "vendor");
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream stream(&file);

            desc.append(stream.readLine());
            desc.append(' ');
            file.close();
        }

        file.setFileName(path + "model");
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream stream(&file);

            desc.append(stream.readLine());
            desc.append(' ');
            file.close();
        }
    }
#endif

    device->setDeviceModel(desc.toAscii().constData());
}

/**
 * Given a media device, add it to our collection
 */
bool MediaMonitorUnix::AddDevice(MythMediaDevice* pDevice)
{
    if ( ! pDevice )
    {
        VERBOSE(VB_IMPORTANT, "Error - MediaMonitorUnix::AddDevice(null)");
        return false;
    }

    // If the user doesn't want this device to be monitored, stop now:
    if (shouldIgnore(pDevice))
        return false;

    QString path = pDevice->getDevicePath();
    if (!path.length())
    {
        VERBOSE(VB_IMPORTANT,
                "MediaMonitorUnix::AddDevice() - empty device path.");
        return false;
    }

    dev_t new_rdev;
    struct stat sb;

    if (stat(path.toLocal8Bit().constData(), &sb) < 0)
    {
        statError(":AddDevice()", path);
        return false;
    }
    new_rdev = sb.st_rdev;

    //
    // Check if this is a duplicate of a device we have already added
    //
    QList<MythMediaDevice*>::const_iterator itr = m_Devices.begin();
    for (; itr != m_Devices.end(); ++itr)
    {
        if (stat((*itr)->getDevicePath().toLocal8Bit().constData(), &sb) < 0)
        {
            statError(":AddDevice()", (*itr)->getDevicePath());
            return false;
        }

        if (sb.st_rdev == new_rdev)
        {
            VERBOSE(VB_MEDIA, LOC + ":AddDevice() - not adding " + path
                              + "\n                        "
                                "because it appears to be a duplicate of "
                              + (*itr)->getDevicePath());
            return false;
        }
    }

    LookupModel(pDevice);

    QMutexLocker locker(&m_DevicesLock);

    connect(pDevice, SIGNAL(statusChanged(MediaStatus, MythMediaDevice*)),
            this, SLOT(mediaStatusChanged(MediaStatus, MythMediaDevice*)));
    m_Devices.push_back( pDevice );
    m_UseCount[pDevice] = 0;
    VERBOSE(VB_MEDIA, LOC + ":AddDevice() - Added " + path);

    return true;
}

// Given a fstab entry to a media device determine what type of device it is
bool MediaMonitorUnix::AddDevice(struct fstab * mep)
{
    if (!mep)
        return false;

    QString devicePath( mep->fs_spec );
    //cout << "AddDevice - " << devicePath << endl;

    MythMediaDevice* pDevice = NULL;
    struct stat sbuf;

    bool is_supermount = false;
    bool is_cdrom = false;

    if (stat(mep->fs_spec, &sbuf) < 0)
       return false;

    //  Can it be mounted?
    if ( ! ( ((strstr(mep->fs_mntops, "owner") &&
        (sbuf.st_mode & S_IRUSR)) || strstr(mep->fs_mntops, "user")) &&
        (strstr(mep->fs_vfstype, MNTTYPE_ISO9660) ||
         strstr(mep->fs_vfstype, MNTTYPE_UDF) ||
         strstr(mep->fs_vfstype, MNTTYPE_AUTO)) ) )
    {
        if (strstr(mep->fs_mntops, MNTTYPE_ISO9660) &&
            strstr(mep->fs_vfstype, MNTTYPE_SUPERMOUNT))
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
            pDevice = MythCDROM::get(this, mep->fs_spec,
                                     is_supermount, m_AllowEject);
    }
    else
    {
        char *dev = 0;
        int len = 0;
        dev = strstr(mep->fs_mntops, SUPER_OPT_DEV);
        if (dev == NULL)
            return false;

        dev += sizeof(SUPER_OPT_DEV)-1;
        while (dev[len] != ',' && dev[len] != ' ' && dev[len] != 0)
            len++;

        if (dev[len] != 0)
        {
            char devstr[256];
            strncpy(devstr, dev, len);
            devstr[len] = 0;
            if (is_cdrom)
                pDevice = MythCDROM::get(this, devstr,
                                         is_supermount, m_AllowEject);
        }
        else
            return false;
    }

    if (pDevice)
    {
        pDevice->setMountPath(mep->fs_file);
        if (pDevice->testMedia() == MEDIAERR_OK)
        {
            if (AddDevice(pDevice))
                return true;
        }
        pDevice->deleteLater();
    }

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

            // skip some sysfs dirs that are _not_ sub-partitions
            if (*pit == "device" || *pit == "holders" || *pit == "queue"
                                 || *pit == "slaves"  || *pit == "subsystem")
                continue;

            found_partitions |= FindPartitions(
                sysfs.absoluteFilePath(*pit), false);
        }

        // no partitions on block device, use main device
        if (!found_partitions)
            found_partitions |= FindPartitions(sysfs.absolutePath(), false);

        return found_partitions;
    }

    QString device_file = GetDeviceFile(dev);

    if (device_file.isNull())
        return false;

    QStringList cdroms = GetCDROMBlockDevices();

    if (cdroms.contains(dev.section('/', -1)))
    {
        // found cdrom device
            pDevice = MythCDROM::get(
                this, device_file.toAscii().constData(), false, m_AllowEject);
    }
    else
    {
        // found block or partition device
            pDevice = MythHDD::Get(
                this, device_file.toAscii().constData(), false, false);
    }

    if (AddDevice(pDevice))
        return true;

    if (pDevice)
        pDevice->deleteLater();

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
    const QStringList list = qBuffer.split('\n', QString::SkipEmptyParts);

    QStringList::const_iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        if ((*it).startsWith("add"))
        {
            QString dev = (*it).section(' ', 1, 1);

            if (CheckRemovable(dev))
                FindPartitions(dev, true);
        }
        else if ((*it).startsWith("remove"))
        {
            RemoveDevice((*it).section(' ', 2, 2));
        }
    }
}
