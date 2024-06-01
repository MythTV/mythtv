// -*- Mode: c++ -*-

// Standard C headers
#include <cstdio>

// POSIX headers
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef ANDROID
#include <fstab.h>
#endif

// UNIX System headers
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>

#include "libmythbase/mythconfig.h"

// Qt headers
#include <QtGlobal>
#if CONFIG_QTDBUS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QVersionNumber>
#include <QXmlStreamReader>
#endif
#include <QList>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>

// MythTV headers
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythcdrom.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythhdd.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"

#include "mediamonitor-unix.h"
#include "mediamonitor.h"

#if HAVE_LIBUDEV
extern "C" {
    #include <libudev.h>
}
#endif


#ifndef Q_OS_ANDROID
#ifndef MNTTYPE_ISO9660
#   ifdef __linux__
    static constexpr const char* MNTTYPE_ISO9660 { "iso9660" };
#   elif defined(__FreeBSD__) || defined(Q_OS_DARWIN) || defined(__OpenBSD__)
    static constexpr const char* MNTTYPE_ISO9660 { "cd9660" };
#   endif
#endif

#ifndef MNTTYPE_UDF
static constexpr const char* MNTTYPE_UDF { "udf" };
#endif

#ifndef MNTTYPE_AUTO
static constexpr const char* MNTTYPE_AUTO { "auto" };
#endif

#ifndef MNTTYPE_SUPERMOUNT
static constexpr const char* MNTTYPE_SUPERMOUNT { "supermount" };
#endif
static const std::string kSuperOptDev { "dev=" };
#endif // !Q_OS_ANDROID

#if CONFIG_QTDBUS
// DBus UDisks2 service - https://udisks.freedesktop.org/
static constexpr const char* UDISKS2_SVC                { "org.freedesktop.UDisks2" };
static constexpr const char* UDISKS2_SVC_DRIVE          { "org.freedesktop.UDisks2.Drive" };
static constexpr const char* UDISKS2_SVC_BLOCK          { "org.freedesktop.UDisks2.Block" };
static constexpr const char* UDISKS2_SVC_FILESYSTEM     { "org.freedesktop.UDisks2.Filesystem" };
static constexpr const char* UDISKS2_SVC_MANAGER        { "org.freedesktop.UDisks2.Manager" };
static constexpr const char* UDISKS2_PATH               { "/org/freedesktop/UDisks2" };
static constexpr const char* UDISKS2_PATH_MANAGER       { "/org/freedesktop/UDisks2/Manager" };
static constexpr const char* UDISKS2_PATH_BLOCK_DEVICES { "/org/freedesktop/UDisks2/block_devices" };
static constexpr const char* UDISKS2_MIN_VERSION        { "2.7.3" };
#endif


// Some helpers for debugging:

static const QString LOC = QString("MMUnix:");

#ifndef Q_OS_ANDROID
// TODO: are these used?
static void fstabError(const QString &methodName)
{
    LOG(VB_GENERAL, LOG_ALERT,
             LOC + methodName + " Error: failed to open " + _PATH_FSTAB +
             " for reading, " + ENO);
}
#endif

static void statError(const QString &methodName, const QString &devPath)
{
    LOG(VB_GENERAL, LOG_ALERT,
             LOC + methodName + " Error: failed to stat " + devPath +
             ", " + ENO);
}

////////////////////////////////////////////////////////////////////////
// MediaMonitor


MediaMonitorUnix::MediaMonitorUnix(QObject* par,
                                   unsigned long interval, bool allowEject)
                : MediaMonitor(par, interval, allowEject)
{
    CheckFileSystemTable();
    if (!gCoreContext->GetBoolSetting("MonitorDrives", false)) {
        LOG(VB_GENERAL, LOG_NOTICE, "MediaMonitor disabled by user setting.");
    }
    else
    {
        CheckMountable();
    }

    LOG(VB_MEDIA, LOG_INFO, LOC +"Initial device list...\n" + listDevices());
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
#ifndef Q_OS_ANDROID
    struct fstab * mep = nullptr;

    // Attempt to open the file system descriptor entry.
    if (!setfsent())
    {
        fstabError(":CheckFileSystemTable()");
        return false;
    }

    // Add all the entries
    while ((mep = getfsent()) != nullptr)
        AddDevice(mep);

    endfsent();

    return !m_devices.isEmpty();
#else
    return false;
#endif
}

#if CONFIG_QTDBUS
// Get a device property by name
static QVariant DriveProperty(const QDBusObjectPath& o, const std::string& kszProperty)
{
    QVariant v;
    QDBusInterface iface(UDISKS2_SVC, o.path(), UDISKS2_SVC_DRIVE,
                         QDBusConnection::systemBus() );
    if (iface.isValid())
    {
        v = iface.property(kszProperty.c_str());
        LOG(VB_MEDIA, LOG_DEBUG, LOC +
            "Udisks2:Drive:" + kszProperty.c_str() + " : " + 
            (v.canConvert<QStringList>() ? v.toStringList().join(", ") : v.toString()) );
    }
    return v;
}

// Uses a QDBusObjectPath for a block device as input parameter
static bool DetectDevice(const QDBusObjectPath& entry, MythUdisksDevice& device,
                         QString& desc, QString& dev)
{
    QDBusInterface block(UDISKS2_SVC, entry.path(), UDISKS2_SVC_BLOCK,
                         QDBusConnection::systemBus() );

    if (!block.property("HintSystem").toBool() &&
        !block.property("HintIgnore").toBool())
    {
        dev = block.property("Device").toString();
        LOG(VB_MEDIA, LOG_DEBUG, LOC +
            "DetectDevice: Device found: " + dev);

        // ignore floppies, too slow
        if (dev.startsWith("/dev/fd"))
            return false;

        // Check if device has partitions
        QDBusInterface properties(UDISKS2_SVC, entry.path(),
            "org.freedesktop.DBus.Properties", QDBusConnection::systemBus());

        auto mountpointscall = properties.call("Get", UDISKS2_SVC_FILESYSTEM,
                                               "MountPoints");
        bool isfsmountable = (properties.lastError().type() == QDBusError::NoError);

        LOG(VB_MEDIA, LOG_DEBUG, LOC +
            QString("     DetectDevice:Entry:isfsmountable : %1").arg(isfsmountable));

        // Get properties of the corresponding drive
        // Note: the properties 'Optical' and 'OpticalBlank' needs a medium inserted
        auto drivePath = block.property("Drive").value<QDBusObjectPath>();
        desc = DriveProperty(drivePath, "Vendor").toString();
        if (!desc.isEmpty())
            desc += " ";
        desc += DriveProperty(drivePath, "Model").toString();
        LOG(VB_MEDIA, LOG_DEBUG, LOC +
            QString("DetectDevice: Found drive '%1'").arg(desc));
        const auto media = DriveProperty(drivePath, "MediaCompatibility").toStringList();
        const bool isOptical = !media.filter("optical", Qt::CaseInsensitive).isEmpty();

        if (DriveProperty(drivePath, "Removable").toBool())
        {
            if (isOptical)
            {
                device = UDisks2DVD;
                return true;
            }
            if (isfsmountable)
            {
                device = UDisks2HDD;
                return true;
            }
        }
    }
    return false;
}
#endif  // CONFIG_QTDBUS


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
        // Connect to UDisks2.  This can sometimes fail if mythfrontend
        // is started during system init
        QDBusInterface ifacem(UDISKS2_SVC, UDISKS2_PATH_MANAGER,
                              UDISKS2_SVC_MANAGER, QDBusConnection::systemBus() );

        if (ifacem.isValid())
        {
            if (!ifacem.property("Version").toString().isEmpty())
            {
                // Minimal supported UDisk2 version: UDISKS2_MIN_VERSION
                QString mversion = ifacem.property("Version").toString();
                QVersionNumber this_version = QVersionNumber::fromString(mversion);
                QVersionNumber min_version = QVersionNumber::fromString(UDISKS2_MIN_VERSION);
                LOG(VB_MEDIA, LOG_DEBUG, LOC + "Using UDisk2 version " + mversion);

                if (QVersionNumber::compare(this_version, min_version) < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        "CheckMountable: UDisks2 version too old: " + mversion);
                    return false;
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Cannot retrieve UDisks2 version, stopping device discovery");
                    return false;
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Cannot interface to UDisks2, will retry");
            continue;
        }

        // Get device paths
        using QDBusObjectPathList = QList<QDBusObjectPath>;
        QDBusPendingReply<QDBusObjectPathList> reply = ifacem.call("GetBlockDevices",
                                                                   QVariantMap{});
        reply.waitForFinished();
        if (!reply.isValid())
        {
            LOG(VB_GENERAL, LOG_ALERT, LOC +
                "CheckMountable DBus GetBlockDevices error: " +
                     reply.error().message() );
            continue;
        }

        LOG(VB_MEDIA, LOG_DEBUG, LOC +
            "CheckMountable: Start listening on UDisks2 DBus");

        // Listen on DBus for UDisk add/remove device messages
        (void)QDBusConnection::systemBus().connect(UDISKS2_SVC, UDISKS2_PATH,
            "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
            this, SLOT(deviceAdded(QDBusObjectPath,QMap<QString,QVariant>)) );
        (void)QDBusConnection::systemBus().connect(UDISKS2_SVC, UDISKS2_PATH,
            "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
            this, SLOT(deviceRemoved(QDBusObjectPath,QStringList)) );

        // Parse the returned device array
        const QDBusObjectPathList& list(reply.value());
        for (const auto& entry : std::as_const(list))
        {
            // Create the MythMediaDevice
            MythMediaDevice* pDevice = nullptr;
            MythUdisksDevice mythdevice = UDisks2INVALID;
            QString description;
            QString path;

            if (DetectDevice(entry, mythdevice, description, path))
            {
                if (mythdevice == UDisks2DVD)
                {
                    pDevice = MythCDROM::get(this, path.toLatin1(), false, m_allowEject);
                    LOG(VB_MEDIA, LOG_DEBUG, LOC +
                        "deviceAdded: Added MythCDROM: " + path);
                }
                else if (mythdevice == UDisks2HDD)
                {
                    pDevice = MythHDD::Get(this, path.toLatin1(), false, false);
                    LOG(VB_MEDIA, LOG_DEBUG, LOC +
                        "deviceAdded: Added MythHDD: " + path);
                }
            }
            if (pDevice)
            {
                // Set the device model and try to add the device
                pDevice -> setDeviceModel(description.toLatin1().constData());
                if (!MediaMonitorUnix::AddDevice(pDevice))
                    pDevice->deleteLater();
            }
        }

        // Success
        return true;
    }

    // Timed out
    return false;

#elif defined(__linux__)
    // NB needs script in /etc/udev/rules.d
    mkfifo(kUDEV_FIFO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    m_fifo = open(kUDEV_FIFO, O_RDONLY | O_NONBLOCK);

    QDir sysfs("/sys/block");
    sysfs.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    auto list = sysfs.entryList();
    for (const auto& device : std::as_const(list))
    {
        // ignore floppies, too slow
        if (device.startsWith("fd"))
            continue;

        sysfs.cd(device);
        QString path = sysfs.absolutePath();
        if (CheckRemovable(path))
            FindPartitions(path, true);
        sysfs.cdUp();
    }
    return true;
#else // not CONFIG_QTDBUS and not linux
    return false;
#endif
}

#if !CONFIG_QTDBUS
/**
 * \brief  Is /sys/block/dev a removable device?
 */
bool MediaMonitorUnix::CheckRemovable(const QString &dev)
{
#ifdef __linux__
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
    ret.replace(QRegularExpression(".*/"), "/dev/");

#ifdef __linux__
#   if HAVE_LIBUDEV
    // Use libudev to determine the name
    ret.clear();
    struct udev *udev = udev_new();
    if (udev != nullptr)
    {
        struct udev_device *device =
            udev_device_new_from_syspath(udev, sysfs.toLatin1().constData());
        if (device != nullptr)
        {
            const char *name = udev_device_get_devnode(device);

            if (name != nullptr)
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
#   else   // !HAVE_LIBUDEV
    // Use udevadm info to determine the name
    QStringList  args;
    args << "info" << "-q"  << "name"
         << "-rp" << sysfs;

    uint flags = kMSStdOut;
    if (VERBOSE_LEVEL_CHECK(VB_MEDIA, LOG_DEBUG))
        flags |= kMSStdErr;

    // TODO: change this to a MythSystemLegacy on the stack?
    MythSystemLegacy *udevinfo = new MythSystemLegacy("udevinfo", args, flags);
    udevinfo->Run(4s);
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
#   endif // HAVE_LIBUDEV
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
    QDBusInterface blocks(UDISKS2_SVC, UDISKS2_PATH_BLOCK_DEVICES,
            "org.freedesktop.DBus.Introspectable", QDBusConnection::systemBus());

    QDBusReply<QString> reply = blocks.call("Introspect");
    QXmlStreamReader xml_parser(reply.value());

    while (!xml_parser.atEnd())
    {
        xml_parser.readNext();

        if (xml_parser.tokenType() == QXmlStreamReader::StartElement
                && xml_parser.name().toString() == "node")
        {
            const QString &name = xml_parser.attributes().value("name").toString();
#if 0
            LOG(VB_MEDIA, LOG_DEBUG, LOC + "GetCDROMBlockDevices: name: " + name);
#endif
            if (!name.isEmpty())
            {
                QString sbdevices = QString::fromUtf8(UDISKS2_PATH_BLOCK_DEVICES);
                QDBusObjectPath entry {sbdevices + "/" + name};
                MythUdisksDevice mythdevice = UDisks2INVALID;
                QString description;
                QString dev;

                LOG(VB_MEDIA, LOG_DEBUG, LOC +
                        "GetCDROMBlockDevices: path: " + entry.path());

                if (DetectDevice(entry, mythdevice, description, dev))
                {
                    if (mythdevice == UDisks2DVD)
                    {
                        LOG(VB_MEDIA, LOG_DEBUG, LOC +
                            "GetCDROMBlockDevices: Added: " + dev);

                        if (dev.startsWith("/dev/"))
                            dev.remove(0,5);
                        l.push_back(dev);
                    }
                }
            }
        }
    }

#elif defined(__linux__)
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
                l = line.split('\t', Qt::SkipEmptyParts);
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

#if !CONFIG_QTDBUS
static void LookupModel(MythMediaDevice* device)
{
    QString   desc;

#if defined(__linux__)
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
             .arg(device->getRealDevice(), desc) );
    device->setDeviceModel(desc.toLatin1().constData());
}
#endif  //!CONFIG_QTDBUS


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
    if (path.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ALERT,
                "MediaMonitorUnix::AddDevice() - empty device path.");
        return false;
    }

    struct stat sb {};
    if (stat(path.toLocal8Bit().constData(), &sb) < 0)
    {
        statError(":AddDevice()", path);
        return false;
    }
    dev_t new_rdev = sb.st_rdev;

    //
    // Check if this is a duplicate of a device we have already added
    //
    for (const auto *device : std::as_const(m_devices))
    {
        if (stat(device->getDevicePath().toLocal8Bit().constData(), &sb) < 0)
        {
            statError(":AddDevice()", device->getDevicePath());
            return false;
        }

        if (sb.st_rdev == new_rdev)
        {
            LOG(VB_MEDIA, LOG_INFO,
                     LOC + ":AddDevice() - not adding " + path +
                     "\n                        "
                     "because it appears to be a duplicate of " +
                     device->getDevicePath());
            return false;
        }
    }
#if !CONFIG_QTDBUS
    // Device vendor and name already fetched by UDisks2
    LookupModel(pDevice);
#endif
    QMutexLocker locker(&m_devicesLock);

    connect(pDevice, &MythMediaDevice::statusChanged,
            this, &MediaMonitor::mediaStatusChanged);
    m_devices.push_back( pDevice );
    m_useCount[pDevice] = 0;
    LOG(VB_MEDIA, LOG_INFO, LOC + ":AddDevice() - Added " + path);

    return true;
}

// Given a fstab entry to a media device determine what type of device it is
bool MediaMonitorUnix::AddDevice(struct fstab * mep)
{
    if (!mep)
        return false;

#ifndef Q_OS_ANDROID
#if 0
    QString devicePath( mep->fs_spec );
    LOG(VB_GENERAL, LOG_DEBUG, "AddDevice - " + devicePath);
#endif

    MythMediaDevice* pDevice = nullptr;
    struct stat sbuf {};

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
                                     is_supermount, m_allowEject);
    }
    else
    {
        QString dev(mep->fs_mntops);
        int pos = dev.indexOf(QString::fromStdString(kSuperOptDev));
        if (pos == -1)
            return false;
        dev = dev.mid(pos+kSuperOptDev.size());
        static const QRegularExpression kSeparatorRE { "[, ]" };
        QStringList parts = dev.split(kSeparatorRE, Qt::SkipEmptyParts);
        if (parts[0].isEmpty())
            return false;
        pDevice = MythCDROM::get(this, dev, is_supermount, m_allowEject);
    }

    if (pDevice)
    {
        pDevice->setMountPath(mep->fs_file);
        if (pDevice->testMedia() == MEDIAERR_OK)
        {
            if (MediaMonitorUnix::AddDevice(pDevice))
                return true;
        }
        pDevice->deleteLater();
    }
#endif

    return false;
}

#if CONFIG_QTDBUS
/*
 * DBus UDisk AddDevice handler
 */

void MediaMonitorUnix::deviceAdded( const QDBusObjectPath& o,
                                    const QMap<QString, QVariant> &interfaces)
{
    if (!interfaces.contains(QStringLiteral("org.freedesktop.UDisks2.Block")))
        return;

    LOG(VB_MEDIA, LOG_DEBUG, LOC + ":deviceAdded " + o.path());

    // Create the MythMediaDevice
    MythMediaDevice* pDevice = nullptr;
    MythUdisksDevice mythdevice = UDisks2INVALID;
    QString description;
    QString path;

    if (DetectDevice(o, mythdevice, description, path))
    {
        if (mythdevice == UDisks2DVD)
        {
            pDevice = MythCDROM::get(this, path.toLatin1(), false, m_allowEject);
            LOG(VB_MEDIA, LOG_DEBUG, LOC +
                "deviceAdded: Added MythCDROM: " + path);
        }
        else if (mythdevice == UDisks2HDD)
        {
            pDevice = MythHDD::Get(this, path.toLatin1(), false, false);
            LOG(VB_MEDIA, LOG_DEBUG, LOC +
                "deviceAdded: Added MythHDD: " + path);
        }
    }
    if (pDevice)
    {
        pDevice -> setDeviceModel(description.toLatin1().constData());
        if (!MediaMonitorUnix::AddDevice(pDevice))
            pDevice->deleteLater();
    }
}

/*
 * DBus UDisk RemoveDevice handler
 */
void MediaMonitorUnix::deviceRemoved(const QDBusObjectPath& o, const QStringList &interfaces)
{
    if (!interfaces.contains(QStringLiteral("org.freedesktop.UDisks2.Block")))
        return;

    LOG(VB_MEDIA, LOG_INFO, LOC + "deviceRemoved " + o.path());

    QString dev = QFileInfo(o.path()).baseName();
    dev.prepend("/dev/");
    RemoveDevice(dev);
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
    MythMediaDevice* pDevice = nullptr;

    if (checkPartitions)
    {
        // check for partitions
        QDir sysfs(dev);
        sysfs.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

        bool found_partitions = false;
        QStringList parts = sysfs.entryList();
        for (const auto& part : std::as_const(parts))
        {
            // skip some sysfs dirs that are _not_ sub-partitions
            if (part == "device" || part == "holders" || part == "queue"
                                 || part == "slaves"  || part == "subsystem"
                                 || part == "bdi"     || part == "power")
                continue;

            found_partitions |= FindPartitions(
                sysfs.absoluteFilePath(part), false);
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
                this, device_file.toLatin1().constData(), false, m_allowEject);
    }
    else
    {
        // found block or partition device
            pDevice = MythHDD::Get(
                this, device_file.toLatin1().constData(), false, false);
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
    std::string buffer(256,'\0');
    QString qBuffer;

    if (m_fifo == -1)
        return;

    int size = read(m_fifo, buffer.data(), 255);
    while (size > 0)
    {
        // append buffer to QString
        buffer[size] = '\0';
        qBuffer.append(QString::fromStdString(buffer));
        size = read(m_fifo, buffer.data(), 255);
    }
    const QStringList list = qBuffer.split('\n', Qt::SkipEmptyParts);
    for (const auto& notif : std::as_const(list))
    {
        if (notif.startsWith("add"))
        {
            QString dev = notif.section(' ', 1, 1);
            LOG(VB_MEDIA, LOG_INFO, "Udev add " + dev);

            if (CheckRemovable(dev))
                FindPartitions(dev, true);
        }
        else if (notif.startsWith("remove"))
        {
            QString dev = notif.section(' ', 2, 2);
            LOG(VB_MEDIA, LOG_INFO, "Udev remove " + dev);
            RemoveDevice(dev);
        }
    }
}
#endif //!CONFIG_QTDBUS
