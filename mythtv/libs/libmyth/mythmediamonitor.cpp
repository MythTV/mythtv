#include "mythmediamonitor.h"
#include "mythcdrom.h"
#include <qapplication.h>
#include <sys/file.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstdio>
#include <mntent.h>
#include <iostream>
#include <unistd.h>

#include "mythcontext.h"

// For testing
#if 0
#include <cstdlib>
#endif

#ifndef MNTTYPE_ISO9660
#define MNTTYPE_ISO9660 "iso9660"
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
    struct mntent * mep = NULL;
    FILE* vt = NULL;
    
    // Attempt to open the file system descriptor entry.
    if ((vt = setmntent("/etc/fstab", "r")) == NULL) 
    {
        perror("setmntent");
        cerr << "MediaMonitor::addFSTab - Failed to open /etc/fstab for "
                "reading." << endl;
        return false;
    }
    else 
    {
        // Add all the entries
        while ((mep = getmntent(vt))!=NULL)
            addDevice(mep->mnt_fsname);
        
        endmntent(vt);
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

// Given a path to a media device determine what type of device it is and 
// add it to our collection.
bool MediaMonitor::addDevice(const char* devPath ) 
{
    QString devicePath( devPath );
    //cout << "addDevice - " << devicePath << endl;

    MythMediaDevice* pDevice = NULL;

    struct mntent * mep=0;
    FILE* vt;
    char lpath[PATH_MAX];
    struct stat sbuf;
    bool is_supermount = false;
    bool is_cdrom = false;

    // Resolve the simlink for the device.
    int len = readlink(devicePath, lpath, PATH_MAX);
    if (len > 0 && len < PATH_MAX)
        lpath[len] = 0;

    // Attempt to open the file system descriptor entry.
    if ((vt = setmntent("/etc/fstab", "r")) == NULL) 
    {
        cout << "couldn't open the fstab" << endl;
        return false;
    }
    else 
    {
        // Loop over the file system descriptor entry.
        while ((mep = getmntent(vt)) != NULL)  
        {
//             cout << "***************************************************" << endl;
//              cout << "devicePath == " << devicePath << endl;
//              cout << "mep->mnt_fsname == " << mep->mnt_fsname << endl;
//              cout << "lpath == " << lpath << endl; 
//              cout << "strcmp(devicePath, mep->mnt_fsname) == " << strcmp(devicePath, mep->mnt_fsname) << endl;
//              cout << "len ==" << len << endl;
//              cout << "strcmp(lpath, mep->mnt_fsname)==" << strcmp(lpath, mep->mnt_fsname) << endl;
//              cout <<endl << endl;

            // Check to see if this is the same as our passed in device. 
            if ((strcmp(devicePath, mep->mnt_fsname) != 0) && 
                 (len && (strcmp(lpath, mep->mnt_fsname) != 0)))
                continue;

            stat(mep->mnt_fsname, &sbuf);

            if (((strstr(mep->mnt_opts, "owner") && 
                (sbuf.st_mode & S_IRUSR)) || strstr(mep->mnt_opts, "user")) && 
                (strstr(mep->mnt_type, MNTTYPE_ISO9660) || 
                 strstr(mep->mnt_type, MNTTYPE_UDF) || 
                 strstr(mep->mnt_type, MNTTYPE_AUTO))) 
            {
                break;        
            }

            if (strstr(mep->mnt_opts, MNTTYPE_ISO9660) && 
                strstr(mep->mnt_type, MNTTYPE_SUPERMOUNT)) 
            {
                is_supermount = true;
                break;
            }
        }

        endmntent(vt);
    }

    if (mep) 
    {
        if (strstr(mep->mnt_opts, MNTTYPE_ISO9660) || 
            strstr(mep->mnt_type, MNTTYPE_ISO9660)) 
        {
            is_cdrom = true;
            //cout << "Device is a CDROM" << endl;
        }

        if (!is_supermount) 
        {
            if (is_cdrom)
                pDevice = MythCDROM::get(this, QString(mep->mnt_fsname),
                                         is_supermount, m_AllowEject);
        }
        else 
        {
            char *dev;
            int len = 0;
            dev = strstr(mep->mnt_opts, SUPER_OPT_DEV);
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
            pDevice->setMountPath(mep->mnt_dir);
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
    }

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
