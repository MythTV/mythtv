// C header
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

// Qt Headers
#include <qfile.h>
#include <qdir.h>

// MythTV headers
#include "mythmedia.h"
#include "mythconfig.h"
#include "mythcontext.h"
#include "util.h"

using namespace std;

#ifdef USING_MINGW
#   define O_NONBLOCK 0
#endif

#define LOC      QString("MythMediaDevice:")
#define LOC_WARN QString("MythMediaDevice, Warning: ")
#define LOC_ERR  QString("MythMediaDevice, Error: ")

static const QString PATHTO_PMOUNT("/usr/bin/pmount");
static const QString PATHTO_PUMOUNT("/usr/bin/pumount");
static const QString PATHTO_MOUNT("/bin/mount");
static const QString PATHTO_UNMOUNT("/bin/umount");
static const QString PATHTO_MOUNTS("/proc/mounts");

const char* MythMediaDevice::MediaStatusStrings[] =
{
    "MEDIASTAT_ERROR",
    "MEDIASTAT_UNKNOWN",
    "MEDIASTAT_UNPLUGGED",
    "MEDIASTAT_OPEN",
    "MEDIASTAT_NODISK",
    "MEDIASTAT_UNFORMATTED",
    "MEDIASTAT_USEABLE",
    "MEDIASTAT_NOTMOUNTED",
    "MEDIASTAT_MOUNTED"
};

const char* MythMediaDevice::MediaErrorStrings[] =
{
    "MEDIAERR_OK",
    "MEDIAERR_FAILED",
    "MEDIAERR_UNSUPPORTED"
};

MythMediaDevice::MythMediaDevice(QObject* par, const char* DevicePath, 
                                 bool SuperMount,  bool AllowEject) 
               : QObject(par)
{
    m_DevicePath = DevicePath;
    m_AllowEject = AllowEject;
    m_Locked = false;
    m_DeviceHandle = -1;
    m_SuperMount = SuperMount;
    m_Status = MEDIASTAT_UNKNOWN;
    m_MediaType = MEDIATYPE_UNKNOWN;

    QFileInfo fi(DevicePath);
    if (fi.isSymLink())
        m_RealDevice = m_DevicePath.section('/', 0, -2) + "/" + fi.readLink();
    else
        m_RealDevice = QString::null;
}

bool MythMediaDevice::openDevice()
{
    // Sanity check
    if (isDeviceOpen())
        return true;
 
    m_DeviceHandle = open(m_DevicePath, O_RDONLY | O_NONBLOCK);
    
    return isDeviceOpen();
}

bool MythMediaDevice::closeDevice()
{
    // Sanity check
    if (!isDeviceOpen())
        return true;

    int ret = close(m_DeviceHandle);
    m_DeviceHandle = -1;
    
    return (ret != -1) ? true : false;
}

bool MythMediaDevice::isDeviceOpen() const 
{ 
    return (m_DeviceHandle >= 0) ? true : false; 
}

bool MythMediaDevice::performMountCmd(bool DoMount)
{
    if (DoMount && isMounted(true))
    {
        VERBOSE(VB_MEDIA, "MythMediaDevice::performMountCmd(true)"
                          " - Logic Error? Device already mounted.");
        return true;
    }

    if (isDeviceOpen())
        closeDevice();

    if (!m_SuperMount) 
    {
        QString MountCommand;

        // Build a command line for mount/unmount and execute it...
        // Is there a better way to do this?
        if (QFile(PATHTO_PMOUNT).exists() && QFile(PATHTO_PUMOUNT).exists())
            MountCommand = QString("%1 %2")
                .arg((DoMount) ? PATHTO_PMOUNT : PATHTO_PUMOUNT)
                .arg(m_DevicePath);
        else
            MountCommand = QString("%1 %2")
                .arg((DoMount) ? PATHTO_MOUNT : PATHTO_UNMOUNT)
                .arg(m_DevicePath);
    
        VERBOSE(VB_MEDIA, QString("Executing '%1'").arg(MountCommand));
        if (0 == myth_system(MountCommand)) 
        {
            if (DoMount)
            {
                // we cannot tell beforehand what the pmount mount point is
                // so verify the mount status of the device
                isMounted(true);
                m_Status = MEDIASTAT_MOUNTED;
                onDeviceMounted();
                VERBOSE(VB_GENERAL,
                        QString("Detected MediaType ") + MediaTypeString());
            }
            else
                onDeviceUnmounted();
            return true;
        }
        else
        {
            VERBOSE(VB_GENERAL, QString("Failed to mount %1.")
                                       .arg(m_DevicePath));
        }
    } 
    else 
    {
        VERBOSE(VB_MEDIA, "Disk inserted on a supermount device");
        // If it's a super mount then the OS will handle mounting /  unmounting.
        // We just need to give derived classes a chance to perform their 
        // mount / unmount logic.
        if (DoMount)
        {
            onDeviceMounted();
            VERBOSE(VB_GENERAL,
                    QString("Detected MediaType ") + MediaTypeString());
        }
        else
            onDeviceUnmounted();
        return true;
    }
    return false;
}

/** \fn MythMediaDevice::DetectMediaType(void)
 *  \brief Returns guessed media type based on file extensions.
 */
MediaType MythMediaDevice::DetectMediaType(void)
{
    MediaType mediatype = MEDIATYPE_UNKNOWN;
    ext_cnt_t ext_cnt;

    if (!ScanMediaType(m_MountPath, ext_cnt))
    {
        VERBOSE(VB_MEDIA, QString("No files with extensions found in '%1'")
                .arg(m_MountPath));
        return mediatype;
    }

    QMap<uint, uint> media_cnts, media_cnt;

    // convert raw counts to composite mediatype counts
    ext_cnt_t::const_iterator it = ext_cnt.begin();
    for (; it != ext_cnt.end(); ++it)
    {
        ext_to_media_t::const_iterator found = m_ext_to_media.find(it.key());
        if (found != m_ext_to_media.end())
            media_cnts[*found] += *it;
    }

    // break composite mediatypes into constituent components
    QMap<uint, uint>::const_iterator cit = media_cnts.begin();
    for (; cit != media_cnts.end(); ++cit)
    {
        for (uint key = 0, j = 0; key != MEDIATYPE_END; j++)
        {
            if ((key = 1 << j) & cit.key())
                media_cnt[key] += *cit;
        }
    }

    // decide on mediatype based on which one has a handler for > # of files
    uint max_cnt = 0;
    for (cit = media_cnt.begin(); cit != media_cnt.end(); ++cit)
    {
        if (*cit > max_cnt)
        {
            mediatype = (MediaType) cit.key();
            max_cnt   = *cit;
        }
    }

    return mediatype;
}

/**
 *  \brief Recursively scan directories and create an associative array
 *         with the number of times we've seen each extension.
 */
bool MythMediaDevice::ScanMediaType(const QString &directory, ext_cnt_t &cnt)
{
    QDir d(directory);
    if (!d.exists())
        return false;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return false;

    QFileInfoListIterator it(*list);

    for (; it.current(); ++it)
    {
        if (("." == (*it)->fileName()) || (".." == (*it)->fileName()))
            continue;

        if ((*it)->isDir())
        {
            ScanMediaType((*it)->absFilePath(), cnt);
            continue;
        }

        const QString ext = (*it)->extension(false);
        if (!ext.isEmpty())
            cnt[ext.lower()]++;
    }

    return !cnt.empty();
}

/** \fn MythMediaDevice::RegisterMediaExtensions(uint,const QString&)
 *  \brief Used to register media types with extensions.
 *
 *  \param mediatype  MediaType flag.
 *  \param extensions Comma separated list of extensions like 'mp3,ogg,flac'.
 */
void MythMediaDevice::RegisterMediaExtensions(uint mediatype,
                                              const QString &extensions)
{
    const QStringList list = QStringList::split(",", extensions, "");
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        m_ext_to_media[*it] |= mediatype;
}

MediaError MythMediaDevice::eject(bool open_close)
{
    (void) open_close;

#ifdef CONFIG_DARWIN
    // Backgrounding this is a bit naughty, but it can take up to five
    // seconds to execute, and freezing the frontend for that long is bad

    QString  command = "disktool -e " + m_DevicePath + " &";

    if (myth_system(command) > 0)
        return MEDIAERR_FAILED;

    return MEDIAERR_OK;
#endif

    return MEDIAERR_UNSUPPORTED;
}

bool MythMediaDevice::isSameDevice(const QString &path)
{
    return (path == m_DevicePath);
}

void MythMediaDevice::setSpeed(int speed)
{
    (void)speed;
    return;
}

MediaError MythMediaDevice::lock() 
{ 
    // We just open the device here, which may or may not do the trick,
    // derived classes can do more...
    if (openDevice()) 
    {
        m_Locked = true;
        return MEDIAERR_OK;
    }
    m_Locked = false;
    return MEDIAERR_FAILED;
}

MediaError MythMediaDevice::unlock()
{ 
    m_Locked = false;
    
    return MEDIAERR_OK;
}

/// \brief Tells us if m_DevicePath is a mounted device.
bool MythMediaDevice::isMounted(bool Verify)
{
    if (!Verify)
        return (m_Status == MEDIASTAT_MOUNTED);

    if (m_DevicePath.isEmpty())
    {
        VERBOSE(VB_MEDIA, LOC + ":isMounted() - logic error, no device path");
        return false;
    }

    QFile mountFile(PATHTO_MOUNTS);

    // Try to open the mounts file so we can search it for our device.
    if (!mountFile.open(IO_ReadOnly))
        return false;

    QString     debug;
    QTextStream stream(&mountFile);

    while (!stream.eof()) 
    {
        QString mountPoint;
        QString deviceName;


        // Extract the mount point and device name.
        stream >> deviceName >> mountPoint;
        stream.readLine(); // skip the rest of the line

        if (deviceName.isEmpty())
            continue;

        if (!deviceName.startsWith("/dev/"))
            continue;

        QStringList deviceNames = deviceName;


        // Get some basic info on the device name, if it looks like a path
        QFileInfo fi(deviceName);
        QString    link = QString::null;

        // If the device name in the mounts file is a symlink, follow it..
        if (fi.isSymLink() && !(link = fi.readLink()).isEmpty())
        {
            if (link[0] == '/') // absolute link
                deviceNames.push_back(link);
            else // relative link..
                deviceNames.push_back(fi.dir(true).absPath() + "/" + link);
        }


        // Deal with escaped spaces
        if (mountPoint.contains("\\040"))
            mountPoint.replace("\\040", " ");


        if (deviceNames.contains(m_DevicePath) ||
            deviceNames.contains(m_RealDevice)  )
        {
            m_MountPath = mountPoint;
            mountFile.close();
            return true;
        }

        if (print_verbose_messages & VB_MEDIA)
            debug += QString("                 %1 | %2\n")
                     .arg(deviceName, 16).arg(mountPoint);
    }

    mountFile.close();

    if (print_verbose_messages & VB_MEDIA)
    {
        debug = LOC + ":isMounted() - mount of '"
                + m_DevicePath + "' not found.\n"
                + "                 Device name/type | Current mountpoint\n"
                + "                 -----------------+-------------------\n"
                + debug
                + "                 =================+===================";
        VERBOSE(VB_MEDIA, debug);
    }

    return false;
}

MediaStatus MythMediaDevice::setStatus( MediaStatus NewStatus, bool CloseIt )
{
    MediaStatus OldStatus = m_Status;

    m_Status = NewStatus;

    // If the status is changed we need to take some actions
    // depending on the old and new status.
    if (NewStatus != OldStatus) 
    {
        switch (NewStatus) 
        {
            // the disk is not / should not be mounted.
            case MEDIASTAT_ERROR:
            case MEDIASTAT_OPEN:
            case MEDIASTAT_NODISK:
            case MEDIASTAT_NOTMOUNTED:
                if (isMounted(true))
                    unmount();
                break;
            case MEDIASTAT_UNKNOWN:
            case MEDIASTAT_USEABLE:
            case MEDIASTAT_MOUNTED:
            case MEDIASTAT_UNPLUGGED:
                // get rid of the compiler warning...
                break;
        }
        
        // Don't fire off transitions to / from unknown states
        if (m_Status != MEDIASTAT_UNKNOWN && OldStatus != MEDIASTAT_UNKNOWN)
            emit statusChanged(OldStatus, this);
    }


    if (CloseIt)
        closeDevice();

    return m_Status;
}

void MythMediaDevice::clearData()
{
    m_VolumeID = QString::null;
    m_KeyID = QString::null;
    m_MediaType = MEDIATYPE_UNKNOWN;
}

const char* MythMediaDevice::MediaTypeString()
{
    return MediaTypeString(m_MediaType);
}

const char* MythMediaDevice::MediaTypeString(MediaType type)
{
    // MediaType is currently a bitmask. If it is ever used as such,
    // this code will only output the main type.

    if (type == MEDIATYPE_UNKNOWN)
        return "MEDIATYPE_UNKNOWN";
    if (type & MEDIATYPE_DATA)
        return "MEDIATYPE_DATA";
    if (type & MEDIATYPE_MIXED)
        return "MEDIATYPE_MIXED";
    if (type & MEDIATYPE_AUDIO)
        return "MEDIATYPE_AUDIO";
    if (type & MEDIATYPE_DVD)
        return "MEDIATYPE_DVD";
    if (type & MEDIATYPE_VCD)
        return "MEDIATYPE_VCD";
    if (type & MEDIATYPE_MMUSIC)
        return "MEDIATYPE_MMUSIC";
    if (type & MEDIATYPE_MVIDEO)
        return "MEDIATYPE_MVIDEO";
    if (type & MEDIATYPE_MGALLERY)
        return "MEDIATYPE_MGALLERY";

    return "MEDIATYPE_UNKNOWN";
};

