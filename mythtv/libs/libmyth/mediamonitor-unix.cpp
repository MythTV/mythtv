// -*- Mode: c++ -*-
#include "config.h"

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
#if CONFIG_QTDBUS
#include <QtDBus>
#include <QDBusConnection>
#endif
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
#include "mythlogging.h"
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

#if CONFIG_QTDBUS
// DBus UDisk service - http://hal.freedesktop.org/docs/udisks/
#define UDISKS_SVC      "org.freedesktop.UDisks"
#define UDISKS_PATH     "/org/freedesktop/UDisks"
#define UDISKS_IFACE    "org.freedesktop.UDisks"
#define UDISKS_DEVADD   "DeviceAdded"
#define UDISKS_DEVRMV   "DeviceRemoved"
#define UDISKS_DEVSIG   "o" // OBJECT_PATH
#endif

const char * MediaMonitorUnix::kUDEV_FIFO = "/tmp/mythtv_media";


// Some helpers for debugging:

static const QString LOC = QString("MMUnix:");

// TODO: are these used?
static void fstabError(const QString &methodName)
{
    LOG(VB_GENERAL, LOG_ALERT, 
             LOC + methodName + " Error: failed to open " + _PATH_FSTAB +
             " for reading, " + ENO);
}

static void statError(const QString &methodName, const QString devPath)
{
    LOG(VB_GENERAL, LOG_ALERT, 
             LOC + methodName + " Error: failed to stat " + devPath + 
             ", " + ENO);
}

////////////////////////////////////////////////////////////////////////
// MediaMonitor


MediaMonitorUnix::MediaMonitorUnix(QObject* par,
                                   unsigned long interval, bool allowEject)
                : MediaMonitor(par, interval, allowEject), m_fifo(-1)
{
    CheckFileSystemTable();
    CheckMountable();

    LOG(VB_MEDIA, LOG_INFO, "Initial device list...\n" + listDevices());
}


#if !CONFIG_QTDBUS
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
#endif // !CONFIG_QTDBUS


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

#if CONFIG_QTDBUS
// Get a device property by name
static QVariant DeviceProperty(const QDBusObjectPath& o, const char kszProperty[])
{
    QVariant v;

    QDBusInterface iface(UDISKS_SVC, o.path(), UDISKS_IFACE".Device",
        QDBusConnection::systemBus() );
    if (iface.isValid())
        v = iface.property(kszProperty);

    return v;
}
#endif

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
#if CONFIG_QTDBUS
    for (int i = 0; i < 10; ++i, usleep(500000))
    {
        // Connect to UDisks.  This can sometimes fail if mythfrontend
        // is started during system init
        QDBusInterface iface(UDISKS_SVC, UDISKS_PATH, UDISKS_IFACE,
            QDBusConnection::systemBus() );
        if (!iface.isValid())
        {
            LOG(VB_GENERAL, LOG_ALERT, LOC +
                "CheckMountable: DBus interface error: " +
                     iface.lastError().message() );
            continue;
        }

        // Enumerate devices
        typedef QList<QDBusObjectPath> QDBusObjectPathList;
        QDBusReply<QDBusObjectPathList> reply = iface.call("EnumerateDevices");
        if (!reply.isValid())
        {
            LOG(VB_GENERAL, LOG_ALERT, LOC +
                "CheckMountable DBus EnumerateDevices error: " +
                     reply.error().message() );
            continue;
        }

        // Listen on DBus for UDisk add/remove device messages
        (void)QDBusConnection::systemBus().connect(
            UDISKS_SVC, UDISKS_PATH, UDISKS_IFACE, UDISKS_DEVADD, UDISKS_DEVSIG,
            this, SLOT(deviceAdded(QDBusObjectPath)) );
        (void)QDBusConnection::systemBus().connect(
            UDISKS_SVC, UDISKS_PATH, UDISKS_IFACE, UDISKS_DEVRMV, UDISKS_DEVSIG,
            this, SLOT(deviceRemoved(QDBusObjectPath)) );

        // Parse the returned device array
        const QDBusObjectPathList& list(reply.value());
        for (QDBusObjectPathList::const_iterator it = list.begin();
            it != list.end(); ++it)
        {
            if (!DeviceProperty(*it, "DeviceIsSystemInternal").toBool() &&
                !DeviceProperty(*it, "DeviceIsPartitionTable").toBool() )
            {
                QString dev = DeviceProperty(*it, "DeviceFile").toString();

                // ignore floppies, too slow
                if (dev.startsWith("/dev/fd"))
                    continue;

                MythMediaDevice* pDevice;
                if (DeviceProperty(*it, "DeviceIsRemovable").toBool())
                    pDevice = MythCDROM::get(this, dev.toAscii(), false, m_AllowEject);
                else
                    pDevice = MythHDD::Get(this, dev.toAscii(), false, false);

                if (pDevice && !AddDevice(pDevice))
                    pDevice->deleteLater();
            }
        }

        // Success
        return true;
    }

    // Timed out
    return false;

#elif defined linux
    // NB needs script in /etc/udev/rules.d
    mkfifo(kUDEV_FIFO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    m_fifo = open(kUDEV_FIFO, O_RDONLY | O_NONBLOCK);

    QDir sysfs("/sys/block");
    sysfs.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    QStringList devices = sysfs.entryList();

    for (QStringList::iterator it = devices.begin(); it != devices.end(); ++it)
    {
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
#else // linux
    return false;
#endif
}

#if !CONFIG_QTDBUS
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
                LOG(VB_MEDIA, LOG_DEBUG, msg + c);
                if (c == '1')
                    return true;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ALERT, msg + "failed");
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
    ret.clear();
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
            else
            {
                // This can happen when udev sends an AddDevice for a block
                // device with partitions.  FindPartition locates a partition
                // in sysfs but udev hasn't created the devnode for it yet.
                // Udev will send another AddDevice for the partition later.
                LOG(VB_MEDIA, LOG_DEBUG, msg + " devnode not (yet) known");
            }

            udev_device_unref(device);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ALERT, 
                     msg + " udev_device_new_from_syspath returned NULL");
            ret = "";
        }

        udev_unref(udev);
    }
    else
        LOG(VB_GENERAL, LOG_ALERT, 
                 "MediaMonitorUnix::GetDeviceFile udev_new failed");
  #else   // HAVE_LIBUDEV
    // Use udevadm info to determine the name
    QStringList  args;
    args << "info" << "-q"  << "name"
         << "-rp" << sysfs;

    uint flags = kMSStdOut | kMSBuffered;
    if (VERBOSE_LEVEL_CHECK(VB_MEDIA, LOG_DEBUG))
        flags |= kMSStdErr;

    // TODO: change this to a MythSystem on the stack?
    MythSystem *udevinfo = new MythSystem("udevinfo", args, flags);
    udevinfo->Run(4);
    if( udevinfo->Wait() != GENERIC_EXIT_OK )
    {
        delete udevinfo;
        return ret;
    }

    if (VERBOSE_LEVEL_CHECK(VB_MEDIA, LOG_DEBUG))
    {
        QTextStream estream(udevinfo->ReadAllErr());
        while( !estream.atEnd() )
            LOG(VB_MEDIA, LOG_DEBUG,
                    msg + " - udevadm info error...\n" + estream.readLine());
    }

    QTextStream ostream(udevinfo->ReadAll());
    QString udevLine = ostream.readLine();
    if (!udevLine.startsWith("device not found in database") )
        ret = udevLine;

    delete udevinfo;
  #endif // HAVE_LIBUDEV
#endif // linux

    LOG(VB_MEDIA, LOG_INFO, msg + "->'" + ret + "'");
    return ret;
}
#endif // !CONFIG_QTDBUS

/*
 *  \brief Reads the list devices known to be CD or DVD devices.
 *  \return list of CD and DVD device names.
 */
// pure virtual
QStringList MediaMonitorUnix::GetCDROMBlockDevices(void)
{
    QStringList l;

#if CONFIG_QTDBUS
    QDBusInterface iface(UDISKS_SVC, UDISKS_PATH, UDISKS_IFACE,
        QDBusConnection::systemBus() );
    if (iface.isValid())
    {
        // Enumerate devices
        typedef QList<QDBusObjectPath> QDBusObjectPathList;
        QDBusReply<QDBusObjectPathList> reply = iface.call("EnumerateDevices");
        if (reply.isValid())
        {
            const QDBusObjectPathList& list(reply.value());
            for (QDBusObjectPathList::const_iterator it = list.begin();
                it != list.end(); ++it)
            {
                if (DeviceProperty(*it, "DeviceIsRemovable").toBool())
                {
                    QString dev = DeviceProperty(*it, "DeviceFile").toString();
                    if (dev.startsWith("/dev/"))
                        dev.remove(0,5);
                    l.push_back(dev);
                }
            }
        }
    }

#elif defined linux
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

    LOG(VB_MEDIA, LOG_DEBUG,
             LOC + ":GetCDROMBlockDevices()->'" + l.join(", ") + "'");
    return l;
}

static void LookupModel(MythMediaDevice* device)
{
    QString   desc;

#if CONFIG_QTDBUS
    QDBusInterface iface(UDISKS_SVC, UDISKS_PATH, UDISKS_IFACE,
        QDBusConnection::systemBus() );
    if (iface.isValid())
    {
        QDBusReply<QDBusObjectPath> reply = iface.call(
            "FindDeviceByDeviceFile", device->getRealDevice());
        if (reply.isValid())
        {
            desc = DeviceProperty(reply, "DriveVendor").toString();
            if (!desc.isEmpty())
                desc += " ";
            desc += DeviceProperty(reply, "DriveModel").toString();
        }
    }

#elif defined linux

    // Given something like /dev/hda1, extract hda1
    QString devname = device->getRealDevice().mid(5,5);

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

    LOG(VB_MEDIA, LOG_DEBUG, QString("LookupModel '%1' -> '%2'")
             .arg(device->getRealDevice()).arg(desc) );
    device->setDeviceModel(desc.toAscii().constData());
}

/**
 * Given a media device, add it to our collection
 */
bool MediaMonitorUnix::AddDevice(MythMediaDevice* pDevice)
{
    if ( ! pDevice )
    {
        LOG(VB_GENERAL, LOG_ERR, "MediaMonitorUnix::AddDevice(null)");
        return false;
    }

    // If the user doesn't want this device to be monitored, stop now:
    if (shouldIgnore(pDevice))
        return false;

    QString path = pDevice->getDevicePath();
    if (!path.length())
    {
        LOG(VB_GENERAL, LOG_ALERT,
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
            LOG(VB_MEDIA, LOG_INFO,
                     LOC + ":AddDevice() - not adding " + path + 
                     "\n                        "
                     "because it appears to be a duplicate of " +
                     (*itr)->getDevicePath());
            return false;
        }
    }

    LookupModel(pDevice);

    QMutexLocker locker(&m_DevicesLock);

    connect(pDevice, SIGNAL(statusChanged(MythMediaStatus, MythMediaDevice*)),
            this, SLOT(mediaStatusChanged(MythMediaStatus, MythMediaDevice*)));
    m_Devices.push_back( pDevice );
    m_UseCount[pDevice] = 0;
    LOG(VB_MEDIA, LOG_INFO, LOC + ":AddDevice() - Added " + path);

    return true;
}

// Given a fstab entry to a media device determine what type of device it is
bool MediaMonitorUnix::AddDevice(struct fstab * mep)
{
    if (!mep)
        return false;

    QString devicePath( mep->fs_spec );
#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "AddDevice - " + devicePath);
#endif

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
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, "Device is a CDROM");
#endif
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

#if CONFIG_QTDBUS
/*
 * DBus UDisk AddDevice handler
 */
void MediaMonitorUnix::deviceAdded( QDBusObjectPath o)
{
    LOG(VB_MEDIA, LOG_INFO, LOC + ":deviceAdded " + o.path());

    // Don't add devices with partition tables, just the partitions
    if (!DeviceProperty(o, "DeviceIsPartitionTable").toBool())
    {
        QString dev = DeviceProperty(o, "DeviceFile").toString();

        MythMediaDevice* pDevice;
        if (DeviceProperty(o, "DeviceIsRemovable").toBool())
            pDevice = MythCDROM::get(this, dev.toAscii(), false, m_AllowEject);
        else
            pDevice = MythHDD::Get(this, dev.toAscii(), false, false);

        if (pDevice && !AddDevice(pDevice))
            pDevice->deleteLater();
    }
}

/*
 * DBus UDisk RemoveDevice handler
 */
void MediaMonitorUnix::deviceRemoved( QDBusObjectPath o)
{
    LOG(VB_MEDIA, LOG_INFO, LOC + "deviceRemoved " + o.path());
#if 0 // This fails because the DeviceFile has just been deleted
    QString dev = DeviceProperty(o, "DeviceFile");
    if (!dev.isEmpty())
        RemoveDevice(dev);
#else
    QString dev = QFileInfo(o.path()).baseName();
    dev.prepend("/dev/");
    RemoveDevice(dev);
#endif
}

#else //CONFIG_QTDBUS

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
    LOG(VB_MEDIA, LOG_DEBUG, 
             LOC + ":FindPartitions(" + dev + 
             QString(",%1").arg(checkPartitions ? " true" : " false" ) + ")");
    MythMediaDevice* pDevice = NULL;

    if (checkPartitions)
    {
        // check for partitions
        QDir sysfs(dev);
        sysfs.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

        bool found_partitions = false;
        QStringList parts = sysfs.entryList();
        for (QStringList::iterator pit = parts.begin();
             pit != parts.end(); ++pit)
        {
            // skip some sysfs dirs that are _not_ sub-partitions
            if (*pit == "device" || *pit == "holders" || *pit == "queue"
                                 || *pit == "slaves"  || *pit == "subsystem"
                                 || *pit == "bdi"     || *pit == "power")
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

    if (device_file.isEmpty())
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
    QString qBuffer;

    if (m_fifo == -1)
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
    for (; it != list.end(); ++it)
    {
        if ((*it).startsWith("add"))
        {
            QString dev = (*it).section(' ', 1, 1);
            LOG(VB_MEDIA, LOG_INFO, "Udev add " + dev);

            if (CheckRemovable(dev))
                FindPartitions(dev, true);
        }
        else if ((*it).startsWith("remove"))
        {
            QString dev = (*it).section(' ', 2, 2);
            LOG(VB_MEDIA, LOG_INFO, "Udev remove " + dev);
            RemoveDevice(dev);
        }
    }
}
#endif //!CONFIG_QTDBUS

