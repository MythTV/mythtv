/**
 * \file     mediamonitor-darwin.cpp
 * \brief    MythMediaMonitor for Darwin/Mac OS X.
 * \version  $Id$
 * \author   Andrew Kimpton, Nigel Pearson
 */

#include <QMetaType>

#include "mythmediamonitor.h"
#include "mediamonitor-darwin.h"
#include "mythcdrom.h"
#include "mythhdd.h"

#include "mythverbose.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <DiskArbitration/DiskArbitration.h>


// These aren't external, they are defined in this file.
// The 'extern "C"' forces them in the C namespace, not the C++
extern "C" void diskAppearedCallback(DADiskRef disk, void *context);
extern "C" void diskDisappearedCallback(DADiskRef disk, void *context);
extern "C" void diskChangedCallback(DADiskRef disk,
                                    CFArrayRef keys, void *context);
extern "C" MediaType MediaTypeForBSDName(const char *bsdName);

static mach_port_t sMasterPort;


/**
 * Guess the media that a volume/partition is on
 */
MediaType FindMediaType(io_service_t service)
{
    kern_return_t  kernResult;
    io_iterator_t  iter;
    MediaType      mediaType = MEDIATYPE_UNKNOWN;
    QString        msg = QString("FindMediaType() - ");
    bool           isWholeMedia = false;

    // Create an iterator across all parents of the service object passed in.
    kernResult = IORegistryEntryCreateIterator(service,
                                               kIOServicePlane,
                                               kIORegistryIterateRecursively
                                               | kIORegistryIterateParents,
                                               &iter);

    if (KERN_SUCCESS != kernResult)
        VERBOSE(VB_IMPORTANT, (msg + "IORegistryEntryCreateIterator"
                                   + " returned %1").arg(kernResult));
    else if (!iter)
        VERBOSE(VB_IMPORTANT, msg + "IORegistryEntryCreateIterator"
                                  + " returned a NULL iterator");
    else
    {
        // A reference on the initial service object is released in
        // the do-while loop below, so add a reference to balance
        IOObjectRetain(service);
        
        do
        {
            isWholeMedia = false;
            if (IOObjectConformsTo(service, kIOMediaClass))
            {
                CFTypeRef wholeMedia;

                wholeMedia = IORegistryEntryCreateCFProperty
                             (service, CFSTR(kIOMediaWholeKey), 
                              kCFAllocatorDefault, 0);

                if (NULL == wholeMedia)
                    VERBOSE(VB_IMPORTANT,
                            msg + "Could not retrieve Whole property");
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
MediaType MediaTypeForBSDName(const char *bsdName)
{
    CFMutableDictionaryRef  matchingDict;
    kern_return_t           kernResult;
    io_iterator_t           iter;
    io_service_t            service;
    QString                 msg = QString("MediaTypeForBSDName(%1)")
                                  .arg(bsdName);
    MediaType               mediaType;


    if (!bsdName || !*bsdName)
    {
        VERBOSE(VB_IMPORTANT, msg + " - Error. No name supplied?");
        return MEDIATYPE_UNKNOWN;
    }

    matchingDict = IOBSDNameMatching(sMasterPort, 0, bsdName);
    if (NULL == matchingDict)
    {
        VERBOSE(VB_IMPORTANT, msg + " - Error. IOBSDNameMatching()"
                                  + " returned a NULL dictionary.");
        return MEDIATYPE_UNKNOWN;
    }

    // Return an iterator across all objects with the matching
    // BSD node name. Note that there should only be one match!
    kernResult = IOServiceGetMatchingServices(sMasterPort, matchingDict, &iter);

    if (KERN_SUCCESS != kernResult)
    {
        VERBOSE(VB_IMPORTANT, (msg + " - Error. IOServiceGetMatchingServices()"
                                   + " returned %2").arg(kernResult));
        return MEDIATYPE_UNKNOWN;
    }
    if (!iter)
    {
        VERBOSE(VB_IMPORTANT, msg + " - Error. IOServiceGetMatchingServices()"
                                  + " returned a NULL iterator");
        return MEDIATYPE_UNKNOWN;
    }

    service = IOIteratorNext(iter);

    // Release this now because we only expect
    // the iterator to contain a single io_service_t.
    IOObjectRelease(iter);

    if (!service)
    {
        VERBOSE(VB_IMPORTANT, msg + " - Error. IOIteratorNext()"
                                  + " returned a NULL iterator");
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
        return NULL;

    size = CFStringGetLength(name) + 1;
    volName = (char *) malloc(size);
    if (!volName)
    {
        VERBOSE(VB_IMPORTANT,
                QString("getVolName() - Error. Can't malloc(%1)?").arg(size));
        return NULL;
    }

    if (!CFStringGetCString(name, volName, size, kCFStringEncodingUTF8))
    {
        free(volName);
        return NULL;
    }

    return volName;
}

/*
 * Given a DA description, return a compound description to help identify it.
 */
static const QString getModel(CFDictionaryRef diskDetails)
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
    MediaType            mediaType;
    QString              model;
    MonitorThreadDarwin *mtd;
    QString              msg = "diskAppearedCallback() - ";
    char                *volName;


    if (!BSDname)
    {
        VERBOSE(VB_MEDIA, msg + "Skipping non-local device");
        return;
    }

    if (!context)
    {
        VERBOSE(VB_IMPORTANT, msg + "Error. Invoked with a NULL context.");
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
        VERBOSE(VB_MEDIA, msg + "Skipping non-removable " + BSDname);
        CFRelease(details);
        return;
    }

    // Get the volume and model name for more user-friendly interaction
    volName = getVolName(details);
    if (!volName)
    {
        VERBOSE(VB_MEDIA, msg + "No volume name for dev " + BSDname);
        CFRelease(details);
        return;
    }

    model     = getModel(details);
    mediaType = MediaTypeForBSDName(BSDname);
    isCDorDVD = (mediaType == MEDIATYPE_DVD) || (mediaType == MEDIATYPE_AUDIO);


    // We know it is removable, and have guessed the type.
    // Call a helper function to create appropriate objects and insert 

    VERBOSE(VB_MEDIA, QString("Found disk %1 - volume name '%2'.")
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

        VERBOSE(VB_MEDIA, QString("Disk %1 - changed name to '%2'.")
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
    CFDictionaryRef match     = kDADiskDescriptionMatchVolumeMountable;
    DASessionRef    daSession = DASessionCreate(kCFAllocatorDefault);

    IOMasterPort(MACH_PORT_NULL, &sMasterPort);

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
    while (m_Monitor && m_Monitor->IsActive())        
    {
        // Run the run loop for interval (milliseconds) - this will
        // handle any disk arbitration appeared/dissappeared events
        CFRunLoopRunInMode(kCFRunLoopDefaultMode,
                           (float) m_Interval / 1000.0f, false );
    }

    DAUnregisterCallback(daSession, (void(*))diskChangedCallback,     this);
    DAUnregisterCallback(daSession, (void(*))diskDisappearedCallback, this);
    DAUnregisterCallback(daSession, (void(*))diskAppearedCallback,    this);
    CFRelease(daSession);
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

    VERBOSE(VB_MEDIA, (msg + "(%1,%2,'%3',%4)")
                      .arg(devName).arg(volName).arg(model).arg(isCDorDVD));

    if (isCDorDVD)
        media = MythCDROM::get(NULL, devName, true, m_Monitor->m_AllowEject);
    else
        media = MythHDD::Get(NULL, devName, true, false);

    if (!media)
    {
        VERBOSE(VB_IMPORTANT,
                msg + " - Error. Couldn't create MythMediaDevice.");
        return;
    }

    // We store the volume name for user activities like ChooseAndEjectMedia().
    media->setVolumeID(volName);
    media->setDeviceModel(model.toAscii());  // Same for the Manufacturer and model

    // Mac OS X devices are pre-mounted here:
    media->setMountPath((QString("/Volumes/") + volName).toAscii());
    media->setStatus(MEDIASTAT_MOUNTED);

    // This is checked in AddDevice(), but checking earlier means
    // we can avoid scanning all the files to determine its type
    if (m_Monitor->shouldIgnore(media))
        return;

    // We want to use MythMedia's code to work out the mediaType.
    // media->onDeviceMounted() is protected,
    // so to call it indirectly, we pretend to mount it here.
    media->mount();

    m_Monitor->AddDevice(media);
}

void MonitorThreadDarwin::diskRemove(QString devName)
{
    VERBOSE(VB_MEDIA,
            QString("MonitorThreadDarwin::diskRemove(%1)").arg(devName));

    if (m_Monitor->m_SendEvent)
    {
        MythMediaDevice *pDevice = m_Monitor->GetMedia(devName);

        if (pDevice)  // Probably should ValidateAndLock() here?
            pDevice->setStatus(MEDIASTAT_NODISK);
        else
            VERBOSE(VB_MEDIA, "Couldn't find MythMediaDevice: " + devName);
    }

    m_Monitor->RemoveDevice(devName);
}

/**
 * \brief Deal with the user, or another program, renaming a volume
 *
 * iTunes has a habit of renaming the disk volumes and track files
 * after it looks up a disk on the GraceNote CDDB.
 */
void MonitorThreadDarwin::diskRename(const char *devName, const char *volName)
{
    VERBOSE(VB_MEDIA, QString("MonitorThreadDarwin::diskRename(%1,%2)")
                      .arg(devName).arg(volName));

    MythMediaDevice *pDevice = m_Monitor->GetMedia(devName);

    if (m_Monitor->ValidateAndLock(pDevice))
    {
        // Send message to plugins to ignore this drive:
        if (m_Monitor->m_SendEvent)
            pDevice->setStatus(MEDIASTAT_NODISK);

        pDevice->setVolumeID(volName);
        pDevice->setMountPath((QString("/Volumes/") + volName).toAscii());

        // Plugins can now use it again:
        if (m_Monitor->m_SendEvent)
            pDevice->setStatus(MEDIASTAT_USEABLE);

        m_Monitor->Unlock(pDevice);
    }
    else
        VERBOSE(VB_MEDIA, QString("Couldn't find MythMediaDevice: ") + devName);
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
    if (m_Active)
        return;

    if (!m_StartThread)
        return;


    // If something (like the MythMusic plugin) stops and starts monitoring,
    // DiskArbitration would re-add the same drives several times over.
    // So, we make sure the device list is deleted.
    m_Devices.clear();


    if (!m_Thread)
        m_Thread = new MonitorThreadDarwin(this, m_MonitorPollingInterval);

    qRegisterMetaType<MediaStatus>("MediaStatus");

    VERBOSE(VB_MEDIA, "Starting MediaMonitor");
    m_Active = true;
    m_Thread->start();
}

/**
 * \brief Simpler version of MediaMonitorUnix::AddDevice()
 *
 * This doesn't do the stat() or duplicate checking.
 */
bool MediaMonitorDarwin::AddDevice(MythMediaDevice* pDevice)
{
    if ( ! pDevice )
    {
        VERBOSE(VB_IMPORTANT, "Error - MediaMonitor::AddDevice(null)");
        return false;
    }

    // If the user doesn't want this device to be monitored, stop now: 
    if (shouldIgnore(pDevice)) 
        return false;

    m_Devices.push_back( pDevice );
    m_UseCount[pDevice] = 0;


    // Devices on Mac OS X don't change status the way Linux ones do,
    // so we force a status change for mediaStatusChanged() to send an event
    if (m_SendEvent)
    {
        pDevice->setStatus(MEDIASTAT_NODISK);
        connect(pDevice, SIGNAL(statusChanged(MediaStatus, MythMediaDevice*)),
                this, SLOT(mediaStatusChanged(MediaStatus, MythMediaDevice*)));
        pDevice->setStatus(MEDIASTAT_USEABLE);
    }


    return true;
}

/*
 * Given a device, return a compound description to help identify it.
 * We try to find out if it is internal, its manufacturer, and model.
 */
static const QString getModel(io_object_t drive)
{
    QString                 desc;
    CFMutableDictionaryRef  props = NULL;

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
        VERBOSE(VB_IMPORTANT, msg + "No Storage Devices? Unlikely!");
        return list;
    }

    // Create an iterator across all parents of the service object passed in.
    kernResult = IOServiceGetMatchingServices(sMasterPort, devices, &iter);

    if (KERN_SUCCESS != kernResult)
    {
        VERBOSE(VB_IMPORTANT, (msg + "IORegistryEntryCreateIterator"
                                   + " returned %1").arg(kernResult));
        return list;
    }
    if (!iter)
    {
        VERBOSE(VB_IMPORTANT, msg + "IORegistryEntryCreateIterator"
                                  + " returned a NULL iterator");
        return list;
    }

    io_object_t  drive;

    while ((drive = IOIteratorNext(iter)))
    {
        CFMutableDictionaryRef  p = NULL;  // properties of drive

        IORegistryEntryCreateCFProperties(drive, &p, kCFAllocatorDefault, 0);
        if (p)
        {
            const void  *type = CFDictionaryGetValue(p, CFSTR("device-type"));

            if (CFEqual(type, CFSTR("DVD")) || CFEqual(type, CFSTR("CD")))
            {
                QString  desc = getModel(drive);

                list.append(desc);
                VERBOSE(VB_MEDIA, desc.prepend("Found CD/DVD: "));
                CFRelease(p);
            }
        }
        else
            VERBOSE(VB_IMPORTANT, msg + "Could not retrieve drive properties");

        IOObjectRelease(drive);
    }

    IOObjectRelease(iter);

    return list;
}
