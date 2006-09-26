/**
 * \file     mediamonitor-darwin.cpp
 * \brief    MythMediaMonitor for Darwin/Mac OS X.
 *
 * \bug      I am using VB_UPNP for some VERBOSE messages.
 *           Should probably define another level of verbosity?
 *
 * \version  $Id$
 * \author   Andrew Kimpton, Nigel Pearson
 */

#include "mythmediamonitor.h"
#include "mediamonitor-darwin.h"
#include "mythcdrom.h"
#include "mythhdd.h"

#include "mythcontext.h"  // For VERBOSE

#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <DiskArbitration/DiskArbitration.h>
#include <qapplication.h>


// These aren't external, they are defined in this file.
// The 'extern "C"' forces them in the C namespace, not the C++
extern "C" static void diskAppearedCallback(DADiskRef disk, void *context);
extern "C" static void diskDisappearedCallback(DADiskRef disk, void *context);
extern "C" static void diskChangedCallback(DADiskRef disk,
                                           CFArrayRef keys, void *context);
extern "C" static MediaType MediaTypeForBSDName(const char *bsdName);

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
    if (name)
        size = CFStringGetLength(name) + 1;
    else
        size = 9;  // 'Untitled\0'

    volName = (char *) malloc(size);
    if (!volName)
    {
        VERBOSE(VB_IMPORTANT,
                QString("getVolName() - Error. Can't malloc(%1)?").arg(size));
        return NULL;
    }

    if (!name || !CFStringGetCString(name, volName, size,
                                     kCFStringEncodingUTF8))
        strcpy(volName, "Untitled");

    return volName;
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
    MonitorThreadDarwin *mtd;
    QString              msg = "diskAppearedCallback() - ";
    char                *volName;


    if (!BSDname)
    {
        VERBOSE(VB_UPNP, msg + "Skipping non-local device");
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
        VERBOSE(VB_UPNP, msg + "Skipping non-removable " + BSDname);
        CFRelease(details);
        return;
    }

    // Get the volume name for more user-friendly interaction
    volName = getVolName(details);

    mediaType = MediaTypeForBSDName(BSDname);
    isCDorDVD = (mediaType == MEDIATYPE_DVD) || (mediaType == MEDIATYPE_AUDIO);


    // We know it is removable, and have guessed the type.
    // Call a helper function to create appropriate objects and insert 

    VERBOSE(VB_UPNP, QString("Found disk %1 - volume name '%2'.")
                     .arg(BSDname).arg(volName));

    mtd->diskInsert(BSDname, volName, isCDorDVD);

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
                                     const char *volName, bool isCDorDVD)
{
    MythMediaDevice  *media;
    QString           msg = QString("MonitorThreadDarwin::diskInsert(%1,%2,%3)")
                            .arg(devName).arg(volName).arg(isCDorDVD);

    VERBOSE(VB_UPNP, msg);

    if (isCDorDVD)
        media = MythCDROM::get(m_Monitor, devName, true,
                               m_Monitor->m_AllowEject);
    else
        media = MythHDD::Get(m_Monitor, devName, true,
                             m_Monitor->m_AllowEject);

    if (!media)
    {
        VERBOSE(VB_IMPORTANT,
                msg + " - Error. Couldn't create MythMediaDevice.");
        return;
    }

    /// We store the volume name for user activities like ChooseAndEjectMedia().
    media->setVolumeID(volName);

    /// Mac OS X devices are pre-mounted, but we want to use MythMedia's code
    /// to work out the mediaType. media->onDeviceMounted() is protected,
    /// so to call it indirectly, we pretend to mount it here.
    media->setMountPath(QString("/Volumes/") + volName);
    media->mount();

    m_Monitor->AddDevice(media);
}

void MonitorThreadDarwin::diskRemove(QString devName)
{
    VERBOSE(VB_UPNP,
            QString("MonitorThreadDarwin::diskRemove(%1)").arg(devName));

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
    QValueList<MythMediaDevice*>::Iterator i;

    for (i = m_Monitor->m_Devices.begin(); i != m_Monitor->m_Devices.end(); ++i)
    {
        if (m_Monitor->ValidateAndLock(*i))
            if ((*i)->getDevicePath() == devName)
            {
                VERBOSE(VB_UPNP,
                        QString("MonitorThreadDarwin::diskRename(%1,%2)")
                        .arg(devName).arg(volName));

                (*i)->setVolumeID(volName);
                (*i)->setMountPath(QString("/Volumes/") + volName);
            }
        m_Monitor->Unlock(*i);
    }
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

    if (!m_Thread)
    {
        m_Thread = new MonitorThreadDarwin(this, m_MonitorPollingInterval);
    }
    m_Active = true;
    m_Thread->start();
}

/**
 * \brief Simpler version of MediaMonitor::AddDevice()
 *
 * This doesn't do the stat(), connect() or duplicate checking.
 */
bool MediaMonitorDarwin::AddDevice(MythMediaDevice* pDevice)
{
    if ( ! pDevice )
    {
        VERBOSE(VB_IMPORTANT, "Error - MediaMonitor::AddDevice(null)");
        return false;
    }

    /// Devices on Mac OS X don't change status the way Linux ones do,
    /// so we need a different way to inform plugins that a new drive
    /// is available. I think this leaks a little memory?
    QApplication::postEvent((QObject*)gContext->GetMainWindow(),
                            new MediaEvent(MEDIASTAT_USEABLE, pDevice));

    m_Devices.push_back( pDevice );
    m_UseCount[pDevice] = 0;

    return true;
}
