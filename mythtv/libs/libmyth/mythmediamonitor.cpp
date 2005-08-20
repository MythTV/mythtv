#include "mythmediamonitor.h"
#include "mythcdrom.h"
#include <qapplication.h>
#include <sys/file.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstdio>
#include <fstab.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "mythcontext.h"

// For testing
#if 0
#include <cstdlib>
#endif

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

using namespace std;

MediaMonitor *theMonitor = NULL;


MediaMonitor* MediaMonitor::getMediaMonitor()
{
    if (!theMonitor)
    {
        if (gContext->GetNumSetting("MonitorDrives") == 1)
        {
            theMonitor = new MediaMonitor(NULL, 500, true);
            theMonitor->addFSTab();
        }
    }

    return theMonitor;
}

// MonitorThread
MonitorThread::MonitorThread(MediaMonitor* pMon, unsigned long interval) 
             : QThread()
{
    m_Monitor = pMon;
    m_Interval = interval;
}

// Nice and simple, as long as our monitor is valid and active, 
// loop and check it's devices.
void MonitorThread::run()
{
    while (m_Monitor && m_Monitor->isActive())        
    {
        m_Monitor->checkDevices();
        msleep(m_Interval);
    }
}

////////////////////////////////////////////////////////////////////////
// MediaMonitor

MediaMonitor::MediaMonitor(QObject* par, unsigned long interval, 
                           bool allowEject) 
            : QObject(par), m_Thread(NULL, interval)
{
    m_AllowEject = allowEject;
    m_Active = false;
    m_Thread.setMonitor(this);
}

// Loop through the file system table and add any supported devices.
bool MediaMonitor::addFSTab()
{
    struct fstab * mep = NULL;
    
    // Attempt to open the file system descriptor entry.
    if (!setfsent()) 
    {
        perror("setfsent");
        cerr << "MediaMonitor::addFSTab - Failed to open "
                _PATH_FSTAB
                " for reading." << endl;
        return false;
    }
    else 
    {
        // Add all the entries
        while ((mep = getfsent()) != NULL)
            (void) addDevice(mep);
        
        endfsent();
    }

    if (m_Devices.isEmpty())
        return false;
    
    return true;
}

// Given a media deivce add it to our collection.
void MediaMonitor::addDevice(MythMediaDevice* pDevice)
{
    connect(pDevice, SIGNAL(statusChanged(MediaStatus, MythMediaDevice*)), 
            this, SLOT(mediaStatusChanged(MediaStatus, MythMediaDevice*)));
    m_Devices.push_back( pDevice );
}

// Given a fstab entry to a media device determine what type of device it is 
bool MediaMonitor::addDevice(struct fstab * mep) 
{
    QString devicePath( mep->fs_spec );
    //cout << "addDevice - " << devicePath << endl;

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
                 MythCDROM::get(this, QString(devstr),
                                is_supermount, m_AllowEject);
         }
         else
             return false;   
     }
        
     if (pDevice) 
     {
         pDevice->setMountPath(mep->fs_file);
         VERBOSE(VB_ALL, QString("Mediamonitor: Adding %1")
                         .arg(pDevice->getDevicePath()));
         if (pDevice->testMedia() == MEDIAERR_OK) 
         {
             addDevice(pDevice);
             return true;
         }
            else
                delete pDevice;
      }

     return false;
}

// Given a path to a media device determine what type of device it is and 
// add it to our collection.
bool MediaMonitor::addDevice(const char* devPath ) 
{
    QString devicePath( devPath );
    //cout << "addDevice - " << devicePath << endl;

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
        cerr << "MediaMonitor::addDevice - Failed to open "
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
       return addDevice(mep); 

    return false;
}

// Poll the devices in our list.
void MediaMonitor::checkDevices()
{
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

// Start the monitoring thread if needed.
void MediaMonitor::startMonitoring()
{
    // Sanity check
    if (m_Active)
        return;

    m_Active = true;
    m_Thread.start();
}

// Stop the monitoring thread if needed.
void MediaMonitor::stopMonitoring()
{
    // Sanity check
    if (!m_Active)
        return;
    m_Active = false;
    m_Thread.wait();
}

// Ask for available media
QValueList <MythMediaDevice*> MediaMonitor::getMedias(MediaType mediatype)
{
    QValueList <MythMediaDevice*> medias;
    QValueList <MythMediaDevice*>::Iterator itr = m_Devices.begin();
    MythMediaDevice* pDev;
    while (itr!=m_Devices.end())
    {
        pDev = *itr;
        if ((pDev->getMediaType()==mediatype) &&
            ((pDev->getStatus()==MEDIASTAT_USEABLE) ||
            (pDev->getStatus()==MEDIASTAT_MOUNTED)))
            medias.push_back(pDev);
        itr++;
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
    
    VERBOSE(VB_ALL, QString("Media status changed...  New status is: %1 "
                            "old status was %2")
                 .arg(MythMediaDevice::MediaStatusStrings[pMedia->getStatus()])
                 .arg(MythMediaDevice::MediaStatusStrings[oldStatus]));
    // This gets called from outside the main thread so we need to post an event back to the main thread.
    // We're only going to pass up events for useable media...
    if (pMedia->getStatus() == MEDIASTAT_USEABLE || 
        pMedia->getStatus() == MEDIASTAT_MOUNTED) 
    {
        VERBOSE(VB_ALL, QString( "Posting MediaEvent"));
        QApplication::postEvent((QObject*)gContext->GetMainWindow(), 
                                new MediaEvent(oldStatus, pMedia));
    }
}
