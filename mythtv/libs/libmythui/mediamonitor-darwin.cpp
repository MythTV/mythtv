/**
 * \file     mediamonitor-darwin.cpp
 * \brief    MythMediaMonitor for Darwin/Mac OS X.
 * \version  $Id$
 * \author   Andrew Kimpton, Nigel Pearson
 */

#include <QDir>
#include <QMetaType>

#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcdrom.h"
#include "libmythbase/mythhdd.h"
#include "libmythbase/mythlogging.h"

#include "mediamonitor.h"
#include "mediamonitor-darwin.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <DiskArbitration/DiskArbitration.h>

#if !HAVE_IOMAINPORT
#define IOMainPort IOMasterPort
#endif

// These aren't external, they are defined in this file.
// The 'extern "C"' forces them in the C namespace, not the C++
extern "C" void diskAppearedCallback(DADiskRef disk, void *context);
extern "C" void diskDisappearedCallback(DADiskRef disk, void *context);
extern "C" void diskChangedCallback(DADiskRef disk,
                                    CFArrayRef keys, void *context);
extern "C" MythMediaType MediaTypeForBSDName(const char *bsdName);

static mach_port_t sMasterPort;


/**
 * Guess the media that a volume/partition is on
 */
MythMediaType FindMediaType(io_service_t service)
{
    kern_return_t  kernResult;
    io_iterator_t  iter;
    MythMediaType  mediaType = MEDIATYPE_UNKNOWN;
    QString        msg = QString("FindMediaType() - ");

    // Create an iterator across all parents of the service object passed in.
    kernResult = IORegistryEntryCreateIterator(service,
                                               kIOServicePlane,
                                               kIORegistryIterateRecursively
                                               | kIORegistryIterateParents,
                                               &iter);

    if (KERN_SUCCESS != kernResult)
    {
        LOG(VB_GENERAL, LOG_CRIT, msg +
            QString("IORegistryEntryCreateIterator returned %1")
                .arg(kernResult));
    }
    else if (!iter)
    {
        LOG(VB_GENERAL, LOG_CRIT, msg +
            "IORegistryEntryCreateIterator returned NULL iterator");
    }
    else
    {
        // A reference on the initial service object is released in
        // the do-while loop below, so add a reference to balance
        IOObjectRetain(service);

        do
        {
            bool isWholeMedia = false;
            if (IOObjectConformsTo(service, kIOMediaClass))
            {
                CFTypeRef wholeMedia;

                wholeMedia = IORegistryEntryCreateCFProperty
                             (service, CFSTR(kIOMediaWholeKey),
                              kCFAllocatorDefault, 0);

                if (!wholeMedia)
		{
                    LOG(VB_GENERAL, LOG_ALERT, msg +
                        "Could not retrieve Whole property");
                }
                else
                {
                    isWholeMedia = CFBooleanGetValue((CFBooleanRef)wholeMedia);
                    CFRelease(wholeMedia);
                }
            }

            if (isWholeMedia)
            {
                if (IOObjectConformsTo(service, kIODVDMediaClass))
                    mediaType = MEDIATYPE_DVD;
                else if (IOObjectConformsTo(service, kIOCDMediaClass))
                    mediaType = MEDIATYPE_AUDIO;
            }

            IOObjectRelease(service);

        } while ((service = IOIteratorNext(iter))
                 && (mediaType == MEDIATYPE_UNKNOWN));

        IOObjectRelease(iter);
    }
    return mediaType;
}

/**
 * Given a BSD device node name, guess its media type
 */
MythMediaType MediaTypeForBSDName(const char *bsdName)
{
    CFMutableDictionaryRef  matchingDict;
    kern_return_t           kernResult;
    io_iterator_t           iter;
    io_service_t            service;
    QString                 msg = QString("MediaTypeForBSDName(%1)")
                                  .arg(bsdName);
    MythMediaType           mediaType;


    if (!bsdName || !*bsdName)
    {
        LOG(VB_GENERAL, LOG_ALERT, msg + " - No name supplied?");
        return MEDIATYPE_UNKNOWN;
    }

    matchingDict = IOBSDNameMatching(sMasterPort, 0, bsdName);
    if (!matchingDict)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 msg + " - IOBSDNameMatching() returned a NULL dictionary.");
        return MEDIATYPE_UNKNOWN;
    }

    // Return an iterator across all objects with the matching
    // BSD node name. Note that there should only be one match!
    kernResult = IOServiceGetMatchingServices(sMasterPort, matchingDict, &iter);

    if (KERN_SUCCESS != kernResult)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 QString(msg + " - IOServiceGetMatchingServices() returned %2")
                         .arg(kernResult));
        return MEDIATYPE_UNKNOWN;
    }
    if (!iter)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 msg + " - IOServiceGetMatchingServices() returned a NULL "
                       "iterator");
        return MEDIATYPE_UNKNOWN;
    }

    service = IOIteratorNext(iter);

    // Release this now because we only expect
    // the iterator to contain a single io_service_t.
    IOObjectRelease(iter);

    if (!service)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 msg + " - IOIteratorNext() returned a NULL iterator");
        return MEDIATYPE_UNKNOWN;
    }
    mediaType = FindMediaType(service);
    IOObjectRelease(service);
    return mediaType;
}


/**
 * Given a description of a disk, copy and return the volume name
 */
static char * getVolName(CFDictionaryRef diskDetails)
{
    CFStringRef name;
    CFIndex     size;
    char       *volName;

    name = (CFStringRef)
           CFDictionaryGetValue(diskDetails, kDADiskDescriptionVolumeNameKey);

    if (!name)
        return nullptr;

    size = CFStringGetLength(name) + 1;
    volName = (char *) malloc(size);
    if (!volName)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                QString("getVolName() - Can't malloc(%1)?").arg(size));
        return nullptr;
    }

    if (!CFStringGetCString(name, volName, size, kCFStringEncodingUTF8))
    {
        free(volName);
        return nullptr;
    }

    return volName;
}

/*
 * Given a DA description, return a compound description to help identify it.
 */
static QString getModel(CFDictionaryRef diskDetails)
{
    QString     desc;
    const void  *strRef;

    // Location
    if (kCFBooleanTrue ==
        CFDictionaryGetValue(diskDetails,
                             kDADiskDescriptionDeviceInternalKey))
        desc.append("Internal ");

    // Manufacturer
    strRef = CFDictionaryGetValue(diskDetails,
                                  kDADiskDescriptionDeviceVendorKey);
    if (strRef)
    {
        desc.append(CFStringGetCStringPtr((CFStringRef)strRef,
                                          kCFStringEncodingMacRoman));
        desc.append(' ');
    }

    // Product
    strRef = CFDictionaryGetValue(diskDetails,
                               kDADiskDescriptionDeviceModelKey);
    if (strRef)
    {
        desc.append(CFStringGetCStringPtr((CFStringRef)strRef,
                                          kCFStringEncodingMacRoman));
        desc.append(' ');
    }

    // Remove the trailing space
    desc.truncate(desc.length() - 1);

    // and multiple spaces
    desc.remove("  ");

    return desc;
}


/*
 * Callbacks which the Disk Arbitration session invokes
 * whenever a disk comes or goes, or is renamed
 */

void diskAppearedCallback(DADiskRef disk, void *context)
{
    const char          *BSDname = DADiskGetBSDName(disk);
    CFDictionaryRef      details;
    bool                 isCDorDVD;
    MythMediaType        mediaType;
    QString              model;
    MonitorThreadDarwin *mtd;
    QString              msg = "diskAppearedCallback() - ";
    char                *volName;


    if (!BSDname)
    {
        LOG(VB_MEDIA, LOG_INFO, msg + "Skipping non-local device");
        return;
    }

    if (!context)
    {
        LOG(VB_GENERAL, LOG_ALERT, msg + "Error. Invoked with a NULL context.");
        return;
    }

    mtd = reinterpret_cast<MonitorThreadDarwin*>(context);


    // We want to monitor CDs/DVDs and USB cameras or flash drives,
    // but probably not hard disk or network drives. For now, ignore
    // any disk or partitions that are not on removable media.
    // Seems OK for hot-plug USB/FireWire disks (i.e. they are removable)

    details = DADiskCopyDescription(disk);

    if (kCFBooleanFalse ==
        CFDictionaryGetValue(details, kDADiskDescriptionMediaRemovableKey))
    {
        LOG(VB_MEDIA, LOG_INFO, msg + QString("Skipping non-removable %1")
            .arg(BSDname));
        CFRelease(details);
        return;
    }

    // Get the volume and model name for more user-friendly interaction
    volName = getVolName(details);
    if (!volName)
    {
        LOG(VB_MEDIA, LOG_INFO, msg + QString("No volume name for dev %1")
            .arg(BSDname));
        CFRelease(details);
        return;
    }

    model = getModel(details);

    if (model.contains("Disk Image"))
    {
        LOG(VB_MEDIA, LOG_INFO, msg + QString("DMG %1 mounted, ignoring")
            .arg(BSDname));
        CFRelease(details);
        free(volName);
        return;
    }

    mediaType = MediaTypeForBSDName(BSDname);
    isCDorDVD = (mediaType == MEDIATYPE_DVD) || (mediaType == MEDIATYPE_AUDIO);


    // We know it is removable, and have guessed the type.
    // Call a helper function to create appropriate objects and insert

    LOG(VB_MEDIA, LOG_INFO, QString("Found disk %1 - volume name '%2'.")
                      .arg(BSDname).arg(volName));

    mtd->diskInsert(BSDname, volName, model, isCDorDVD);

    CFRelease(details);
    free(volName);
}

void diskDisappearedCallback(DADiskRef disk, void *context)
{
    const char  *BSDname = DADiskGetBSDName(disk);

    if (context)
        reinterpret_cast<MonitorThreadDarwin *>(context)->diskRemove(BSDname);
}

void diskChangedCallback(DADiskRef disk, CFArrayRef keys, void *context)
{
    if (CFArrayContainsValue(keys, CFRangeMake(0, CFArrayGetCount(keys)),
                             kDADiskDescriptionVolumeNameKey))
    {
        const char     *BSDname = DADiskGetBSDName(disk);
        CFDictionaryRef details = DADiskCopyDescription(disk);
        char           *volName = getVolName(details);

        LOG(VB_MEDIA, LOG_INFO, QString("Disk %1 - changed name to '%2'.")
                          .arg(BSDname).arg(volName));

        reinterpret_cast<MonitorThreadDarwin *>(context)
            ->diskRename(BSDname, volName);
        CFRelease(details);
        free(volName);
    }
}


/**
 * Use the DiskArbitration Daemon to inform us of media changes
 */
void MonitorThreadDarwin::run(void)
{
    RunProlog();
    CFDictionaryRef match     = kDADiskDescriptionMatchVolumeMountable;
    DASessionRef    daSession = DASessionCreate(kCFAllocatorDefault);

    IOMainPort(MACH_PORT_NULL, &sMasterPort);

    DARegisterDiskAppearedCallback(daSession, match,
                                   diskAppearedCallback, this);
    DARegisterDiskDisappearedCallback(daSession, match,
                                      diskDisappearedCallback, this);
    DARegisterDiskDescriptionChangedCallback(daSession, match,
                                             kDADiskDescriptionWatchVolumeName,
                                             diskChangedCallback, this);

    DASessionScheduleWithRunLoop(daSession,
                                 CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);


    // Nice and simple, as long as our monitor is valid and active,
    // loop and let daSession check the devices.
    while (m_monitor && m_monitor->IsActive())
    {
        // Run the run loop for interval (milliseconds) - this will
        // handle any disk arbitration appeared/dissappeared events
        CFRunLoopRunInMode(kCFRunLoopDefaultMode,
                           (float) m_interval / 1000.0F, false );
    }

    DAUnregisterCallback(daSession, (void(*))diskChangedCallback,     this);
    DAUnregisterCallback(daSession, (void(*))diskDisappearedCallback, this);
    DAUnregisterCallback(daSession, (void(*))diskAppearedCallback,    this);
    CFRelease(daSession);
    RunEpilog();
}

/**
 * \brief Create a MythMedia instance and insert in MythMediaMonitor list
 *
 * We are a friend class of MythMediaMonitor,
 * so that we can add or remove from its list of media objects.
 */
void MonitorThreadDarwin::diskInsert(const char *devName,
                                     const char *volName,
                                     QString model, bool isCDorDVD)
{
    MythMediaDevice  *media;
    QString           msg = "MonitorThreadDarwin::diskInsert";

    LOG(VB_MEDIA, LOG_DEBUG, msg + QString("(%1,%2,'%3',%4)")
                      .arg(devName).arg(volName).arg(model).arg(isCDorDVD));

    if (isCDorDVD)
        media = MythCDROM::get(nullptr, devName, true, m_monitor->m_allowEject);
    else
        media = MythHDD::Get(nullptr, devName, true, false);

    if (!media)
    {
        LOG(VB_GENERAL, LOG_ALERT, msg + "Couldn't create MythMediaDevice.");
        return;
    }

    // We store the volume name for user activities like ChooseAndEjectMedia().
    media->setVolumeID(volName);
    media->setDeviceModel(model.toLatin1());  // Same for the Manufacturer and model

    // Mac OS X devices are pre-mounted here:
    QString mnt = "/Volumes/"; mnt += volName;
    media->setMountPath(mnt.toLatin1());

    int  attempts = 0;
    QDir d(mnt);
    while (!d.exists())
    {
        LOG(VB_MEDIA, LOG_WARNING,
            (msg + "() - Waiting for mount '%1' to become stable.").arg(mnt));
        usleep(std::chrono::microseconds(120000)); // cppcheck-suppress usleepCalled
        if ( ++attempts > 4 )
            usleep(std::chrono::microseconds(200000)); // cppcheck-suppress usleepCalled
        if ( attempts > 8 )
        {
            delete media;
            LOG(VB_MEDIA, LOG_ALERT, msg + "() - Giving up");
            return;
        }
    }

    media->setStatus(MEDIASTAT_MOUNTED);

    // This is checked in AddDevice(), but checking earlier means
    // we can avoid scanning all the files to determine its type
    if (m_monitor->shouldIgnore(media))
        return;

    // We want to use MythMedia's code to work out the mediaType.
    // media->onDeviceMounted() is protected,
    // so to call it indirectly, we pretend to mount it here.
    media->mount();

    m_monitor->AddDevice(media);
}

void MonitorThreadDarwin::diskRemove(QString devName)
{
    LOG(VB_MEDIA, LOG_DEBUG,
            QString("MonitorThreadDarwin::diskRemove(%1)").arg(devName));

    MythMediaDevice *pDevice = m_monitor->GetMedia(devName);

    if (pDevice)  // Probably should ValidateAndLock() here?
        pDevice->setStatus(MEDIASTAT_NODISK);
    else
        LOG(VB_MEDIA, LOG_INFO, "Couldn't find MythMediaDevice: " + devName);

    m_monitor->RemoveDevice(devName);
}

/**
 * \brief Deal with the user, or another program, renaming a volume
 *
 * iTunes has a habit of renaming the disk volumes and track files
 * after it looks up a disk on the GraceNote CDDB.
 */
void MonitorThreadDarwin::diskRename(const char *devName, const char *volName)
{
    LOG(VB_MEDIA, LOG_DEBUG,
             QString("MonitorThreadDarwin::diskRename(%1,%2)")
                      .arg(devName).arg(volName));

    MythMediaDevice *pDevice = m_monitor->GetMedia(devName);

    if (m_monitor->ValidateAndLock(pDevice))
    {
        // Send message to plugins to ignore this drive:
        pDevice->setStatus(MEDIASTAT_NODISK);

        pDevice->setVolumeID(volName);
        pDevice->setMountPath((QString("/Volumes/") + volName).toLatin1());

        // Plugins can now use it again:
        pDevice->setStatus(MEDIASTAT_USEABLE);

        m_monitor->Unlock(pDevice);
    }
    else
        LOG(VB_MEDIA, LOG_INFO,
                 QString("Couldn't find MythMediaDevice: %1").arg(devName));
}

/**
 * \class MediaMonitorDarwin
 *
 * This currently depends on Apple's DiskArbitration framework.
 * Only recent versions of OS X have this.
 */
void MediaMonitorDarwin::StartMonitoring(void)
{
    // Sanity check
    if (m_active)
        return;

    // If something (like the MythMusic plugin) stops and starts monitoring,
    // DiskArbitration would re-add the same drives several times over.
    // So, we make sure the device list is deleted.
    m_devices.clear();


    if (!m_thread)
        m_thread = new MonitorThreadDarwin(this, m_monitorPollingInterval);

    qRegisterMetaType<MythMediaStatus>("MythMediaStatus");

    LOG(VB_MEDIA, LOG_NOTICE, "Starting MediaMonitor");
    m_active = true;
    m_thread->start();
}

/**
 * \brief Simpler version of MediaMonitorUnix::AddDevice()
 *
 * This doesn't do the stat() or duplicate checking.
 */
bool MediaMonitorDarwin::AddDevice(MythMediaDevice* pDevice)
{
    if ( !pDevice )
    {
        LOG(VB_GENERAL, LOG_ERR, "MediaMonitor::AddDevice(null)");
        return false;
    }

    // If the user doesn't want this device to be monitored, stop now:
    if (shouldIgnore(pDevice))
        return false;

    m_devices.push_back( pDevice );
    m_useCount[pDevice] = 0;


    // Devices on Mac OS X don't change status the way Linux ones do,
    // so we force a status change for mediaStatusChanged() to send an event
    pDevice->setStatus(MEDIASTAT_NODISK);
    connect(pDevice, &MythMediaDevice::statusChanged,
            this, &MediaMonitorDarwin::mediaStatusChanged);
    pDevice->setStatus(MEDIASTAT_USEABLE);


    return true;
}

/*
 * Given a device, return a compound description to help identify it.
 * We try to find out if it is internal, its manufacturer, and model.
 */
static QString getModel(io_object_t drive)
{
    QString                 desc;
    CFMutableDictionaryRef  props = nullptr;

    props = (CFMutableDictionaryRef) IORegistryEntrySearchCFProperty(drive, kIOServicePlane, CFSTR(kIOPropertyProtocolCharacteristicsKey), kCFAllocatorDefault, kIORegistryIterateParents | kIORegistryIterateRecursively);
CFShow(props);
    if (props)
    {
        const void  *location = CFDictionaryGetValue(props, CFSTR(kIOPropertyPhysicalInterconnectLocationKey));
        if (CFEqual(location, CFSTR("Internal")))
            desc.append("Internal ");
    }

    props = (CFMutableDictionaryRef) IORegistryEntrySearchCFProperty(drive, kIOServicePlane, CFSTR(kIOPropertyDeviceCharacteristicsKey), kCFAllocatorDefault, kIORegistryIterateParents | kIORegistryIterateRecursively);
    if (props)
    {
        const void *product = CFDictionaryGetValue(props, CFSTR(kIOPropertyProductNameKey));
        const void *vendor  = CFDictionaryGetValue(props, CFSTR(kIOPropertyVendorNameKey));
        if (vendor)
        {
            desc.append(CFStringGetCStringPtr((CFStringRef)vendor, kCFStringEncodingMacRoman));
            desc.append(" ");
        }
        if (product)
        {
            desc.append(CFStringGetCStringPtr((CFStringRef)product, kCFStringEncodingMacRoman));
            desc.append(" ");
        }
    }

    // Omit the trailing space
    desc.truncate(desc.length() - 1);

    return desc;
}

/**
 * \brief List of CD/DVD devices
 *
 * On Unix, this returns a list of /dev nodes which can be open()d.
 * Darwin doesn't have fixed devices for removables/pluggables,
 * so this method is actually useless as it stands.
 * In the long term, this method should return both a name,
 * and an opaque type? (for the IOKit io_object_t)
 */
QStringList MediaMonitorDarwin::GetCDROMBlockDevices()
{
    kern_return_t          kernResult;
    CFMutableDictionaryRef devices;
    io_iterator_t          iter;
    QStringList            list;
    QString                msg = QString("GetCDRomBlockDevices() - ");


    devices = IOServiceMatching(kIOBlockStorageDeviceClass);
    if (!devices)
    {
        LOG(VB_GENERAL, LOG_ALERT, msg + "No Storage Devices? Unlikely!");
        return list;
    }

    // Create an iterator across all parents of the service object passed in.
    kernResult = IOServiceGetMatchingServices(sMasterPort, devices, &iter);

    if (KERN_SUCCESS != kernResult)
    {
        LOG(VB_GENERAL, LOG_ALERT, msg +
            QString("IORegistryEntryCreateIterator returned %1")
                .arg(kernResult));
        return list;
    }
    if (!iter)
    {
        LOG(VB_GENERAL, LOG_ALERT, msg +
            "IORegistryEntryCreateIterator returned a NULL iterator");
        return list;
    }

    io_object_t  drive;

    while ((drive = IOIteratorNext(iter)))
    {
        CFMutableDictionaryRef  p = nullptr;  // properties of drive

        IORegistryEntryCreateCFProperties(drive, &p, kCFAllocatorDefault, 0);
        if (p)
        {
            const void  *type = CFDictionaryGetValue(p, CFSTR("device-type"));

            if (CFEqual(type, CFSTR("DVD")) || CFEqual(type, CFSTR("CD")))
            {
                QString  desc = getModel(drive);

                list.append(desc);
                LOG(VB_MEDIA, LOG_INFO, desc.prepend("Found CD/DVD: "));
                CFRelease(p);
            }
        }
        else
            LOG(VB_GENERAL, LOG_ALERT,
                     msg + "Could not retrieve drive properties");

        IOObjectRelease(drive);
    }

    IOObjectRelease(iter);

    return list;
}
