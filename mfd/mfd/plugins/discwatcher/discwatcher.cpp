/*
	discwatcher.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    Methods for an optical disc watcher
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <fcntl.h>
#include <unistd.h>


#include <qfileinfo.h>
              
#include "settings.h"

#include "discwatcher.h"

DiscWatcher *disc_watcher = NULL;

DiscWatcher::DiscWatcher(MFD *owner, int identity)
            :MFDServicePlugin(owner, identity, 0, "discwatcher", false)
{
    device_watchers.setAutoDelete(true);
}

void DiscWatcher::run()
{

    //
    //  Before we get in our event loop, build a list of devices to watch 
    //
    
    buildDiscList();

    //
    //  Just sit in a timed select call, wake up, check disc(s), go back to
    //  sleep. We use the pipe only as a way for the core mfd to wake us up
    //  (if, for example, it's time to shutdown)
    //
    
    while(keep_going)
    {

        //
        //  Check the disc(s)
        //
        
        checkDiscs();

        //
        //  Zero out what we're going to watch in select
        //

        int nfds = 0;
        fd_set readfds;
        FD_ZERO(&readfds);

        //
        //  Add the control pipe
        //
            
        FD_SET(u_shaped_pipe[0], &readfds);
        if (nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    
        timeout_mutex.lock();
            timeout.tv_sec = 5;    //  Configurable ... FIX
            timeout.tv_usec = 0;
        timeout_mutex.unlock();

        //
        //  Sit in select() until data arrives
        //
    
        int result = select(nfds, &readfds, NULL, NULL, &timeout);
        if (result < 0)
        {
            warning("got an error on select()");
        }
        
        //
        //  In case data came in on out u_shaped_pipe, clean it out
        //

        if (FD_ISSET(u_shaped_pipe[0], &readfds))
        {
            u_shaped_pipe_mutex.lock();
                char read_back[2049];
                read(u_shaped_pipe[0], read_back, 2048);
            u_shaped_pipe_mutex.unlock();
        }
    }
    
    //
    //  I'm shutting down, so I need to clean out all the device watchers
    //  and any data associated with them
    //

    QPtrListIterator<DeviceWatcher> it( device_watchers );
    DeviceWatcher *a_device_watcher;
    while ( (a_device_watcher = it.current()) != 0 )
    {
        ++it;
        a_device_watcher->removeAllMetadata();
    }
}

void DiscWatcher::buildDiscList()
{
    //
    //  Get a list of devices to watch
    //    
    
    QStringList devices_to_watch = 
            mfdContext->getListSetting(
                                "OpticalDrivesToWatch",
                                QStringList::split(':',"/dev/cdrom:/dev/dvd")
                                      );

    //
    //  Anything where the file pointing to a device doesn't even exist
    //  (i.e. there is no /dev/cdrom entry) should not get watched
    //

    struct stat fileinfo;
    for(
        QStringList::Iterator it = devices_to_watch.begin(); 
        it != devices_to_watch.end();
       )
    {
        if (stat(*it, &fileinfo) < 0)
        {
            warning(QString("\"%1\" does not exist, will not be watched")
                    .arg(*it));
            it = devices_to_watch.remove(it);
        }
        else
        {
            ++it;
        }
    }

    //
    //  Ok, the /dev (device) entries exist, are there any devices there? If
    //  they're there, we should be able to open them. If we can open them,
    //  are they actually CD/DVD devices?
    //    
    
    for(
        QStringList::Iterator it = devices_to_watch.begin(); 
        it != devices_to_watch.end();
       )
    {
        int device_handle = open(*it, O_RDONLY | O_NONBLOCK);
        if (device_handle > 0)
        {
            //
            //  Device exists and can be opened
            //

            if (ioctl(device_handle, CDROM_GET_CAPABILITY, NULL) < 0)
            {
                //
                //  Device is not a CD/DVD
                //
                warning(QString("device at \"%1\" is not a cd or dvd "
                                "drive, will not be watched")
                                .arg(*it));
                it = devices_to_watch.remove(it);
            }
            else
            {
                //
                //  All is well
                //

                ++it;
            }
            close(device_handle);
        }
        else
        {
            warning(QString("device at \"%1\" does not exist or has "
                            "nasty permissions, will not be watched")
                            .arg(*it));
            it = devices_to_watch.remove(it);
        }
    }

    //
    //  If one thing is a symlink for another thing (ie. both /dev/cdrom and
    //  /dev/dvd point at the same thing), we don't want to monitor the same
    //  thing twice.
    //
    //  Note the _classic_ example of thor's inability to look trough lists
    //  efficiently
    //

    QFileInfo thing_a;
    QFileInfo thing_b;
    bool something_changed = true;
    while(something_changed)
    {
        something_changed = false;
        for(uint i=0; i < devices_to_watch.count(); i++)
        {
            //
            //  Set thing_a (first of two things to compare)
            //

            thing_a = QFileInfo(devices_to_watch[i]);
            if (thing_a.isSymLink())
            {
                QString link_name = thing_a.readLink();
                if (!link_name.contains("/"))
                {
                    link_name.prepend("/");
                    link_name.prepend(thing_a.dirPath());
                }
                thing_a = QFileInfo(link_name);
            }
            
            for(uint j=i + 1; j < devices_to_watch.count(); j++)
            {
                //
                //  Set thing_b (second of two things to compare
                //

                thing_b = QFileInfo(devices_to_watch[j]);
                if (thing_b.isSymLink())
                {
                    QString link_name = thing_b.readLink();
                    if (!link_name.contains("/"))
                    {
                        link_name.prepend("/");
                        link_name.prepend(thing_b.dirPath());
                    }
                    thing_b = QFileInfo(link_name);
                }

                
                //
                //  Compare them
                //
                
                if (thing_a.absFilePath() == thing_b.absFilePath())
                {
                    //
                    //  Ha! they match. Remove the second one, then start
                    //  all over again.
                    //
                    
                    log(QString("\"%1\" being removed from things to watch "
                                "since it is the same as \"%2\"")
                                .arg(devices_to_watch[j])
                                .arg(devices_to_watch[i]), 1);
                    something_changed = true;
                    devices_to_watch.remove(devices_to_watch[j]);
                    i = devices_to_watch.count();
                    j = devices_to_watch.count();                    
                }
            }
        } 
    }

    //
    //  Right, we now finally have a list of devices we want to watch. Make
    //  a DeviceWatcher for each of them
    //

    for(
        QStringList::Iterator it = devices_to_watch.begin(); 
        it != devices_to_watch.end(); 
        ++it
       )
    {
        DeviceWatcher *new_device_watcher = new DeviceWatcher(
                                                                this, 
                                                                *it, 
                                                                parent->getMetadataServer(),
                                                                parent
                                                             );
        device_watchers.append(new_device_watcher);
    }
}



void DiscWatcher::checkDiscs()
{
    //
    //  Iterate over our device watchers, and tell them each (in turn) to
    //  update themselves.
    //

    QPtrListIterator<DeviceWatcher> it( device_watchers );
    DeviceWatcher *a_device_watcher;
    while ( (a_device_watcher = it.current()) != 0 )
    {
        ++it;
        a_device_watcher->check();
    }
}

DiscWatcher::~DiscWatcher()
{
    device_watchers.clear();
}
