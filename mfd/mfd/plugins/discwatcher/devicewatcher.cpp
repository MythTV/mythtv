/*
	devicewatcher.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for object to watch a given device
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <fcntl.h>
#include <unistd.h>

#include "mfd_events.h"
#include "cddecoder.h"

#include "devicewatcher.h"
#include "discwatcher.h"

DeviceWatcher::DeviceWatcher(
                                DiscWatcher *owner, 
                                const QString &device, 
                                MetadataServer *the_md_server,
                                MFD* the_mfd
                            )
{
    parent = owner;
    device_to_watch = device;
    my_mfd = the_mfd;

    //
    //  Get a pointer to the metadata server
    //    

    metadata_server = the_md_server;
    current_metadata_container = NULL;

    //
    //  Try and figure out the type of the device I'm watching
    //

 
    cd_device_type = M_CD_DEVICE_TYPE_UNKNOWN;
    dvd_device_type = M_DVD_DEVICE_TYPE_UNKNOWN;
    checkDeviceType();
    
    //
    //  First time through, check the metadata
    //
    
    updateMetadata();
}

void DeviceWatcher::checkDeviceType()
{
    int drive_handle = open(device_to_watch, O_RDONLY | O_NONBLOCK);
    
    if(drive_handle < 1)
    {
        warning(QString("could not get a descriptor to %1")
                .arg(device_to_watch));
                return;
    }
    

    int capabilities = ioctl(drive_handle, CDROM_GET_CAPABILITY, NULL);
    close(drive_handle);

    //
    //  Set the capabilities for this device
    //

    cd_device_type = M_CD_DEVICE_TYPE_NOTHING;
    dvd_device_type = M_DVD_DEVICE_TYPE_NOTHING;
    if( capabilities & CDC_CD_R)
    {
        cd_device_type = M_CD_DEVICE_TYPE_R;
    }
    if( capabilities & CDC_CD_RW)
    {
        cd_device_type = M_CD_DEVICE_TYPE_RW;
    }
    if( capabilities & CDC_DVD)
    {
        dvd_device_type = M_DVD_DEVICE_TYPE_R;
    }
    if( capabilities & CDC_DVD_R)
    {
        dvd_device_type = M_DVD_DEVICE_TYPE_RW;
    }


    //
    //  Build a log entry that describes the device
    //

    
    QString log_string = QString("will watch \"%1\" (").arg(device_to_watch);

    if(cd_device_type == M_CD_DEVICE_TYPE_R)
    {
        log_string.append("cd r ");
    }
    else if (cd_device_type == M_CD_DEVICE_TYPE_RW)
    {
        log_string.append("cd rw ");
    }
    else
    {
        log_string.append("no cd ");
    }
    
    log_string.append("and");
    
    if(dvd_device_type == M_DVD_DEVICE_TYPE_R)
    {
        log_string.append(" dvd r ");
    }
    else if (dvd_device_type == M_DVD_DEVICE_TYPE_RW)
    {
        log_string.append(" dvd rw ");
    }
    else
    {
        log_string.append(" no dvd ");
    }

    log_string.append("capabilities)");
    
    log(log_string, 1);
}

void DeviceWatcher::check()
{
    //
    //  Look at my device, see if anything new is going on
    //
    
    int drive_handle = open(device_to_watch, O_RDONLY | O_NONBLOCK);
    
    if(drive_handle < 1)
    {
        warning(QString("could not get a descriptor to %1")
                .arg(device_to_watch));
        removeAllMetadata();
        return;
    }

    //
    //  See if there is media there at all
    //  

    int status = ioctl(drive_handle, CDROM_DRIVE_STATUS, NULL);
    if(status < 4)
    {
        //
        // -1  error (no disc)
        //  1 = no info
        //  2 = tray open
        //  3 = not ready
        //

        close(drive_handle);
        removeAllMetadata();
        return;
    }

    status = ioctl(drive_handle, CDROM_MEDIA_CHANGED, NULL);
    close(drive_handle);

    if(!status)
    {
        //
        //  Media has not changed, we're done
        //
        
        return;
    }
    
    //
    //  Media has changed, so update the metadata
    //

    updateMetadata();
}

void DeviceWatcher::removeAllMetadata()
{
    //
    //  If we have any metadata registered with the metadata server, remove
    //  it. All of it.
    //

    if(current_metadata_container)
    {
        metadata_server->deleteContainer(current_metadata_container->getIdentifier());
        current_metadata_container = NULL;
    }        
}

void DeviceWatcher::updateMetadata()
{
    //
    //  The disc has changed, begin by removing any previous metadata
    //
    
    removeAllMetadata();
    
    //
    //  Next, figure out what kind of media we're dealing with here
    //
    
    int drive_handle = open(device_to_watch, O_RDONLY | O_NONBLOCK);
    
    if(drive_handle < 1)
    {
        warning(QString("could not get a descriptor to %1")
                .arg(device_to_watch));
        return;
    }

    int media_type = ioctl(drive_handle, CDROM_DISC_STATUS, NULL);    
    close(drive_handle);
        
    switch(media_type)
    {
        case CDS_NO_INFO:
        case CDS_NO_DISC:
        case CDS_TRAY_OPEN:
        case CDS_DRIVE_NOT_READY:
        
            return;
            break;
         
        case CDS_AUDIO:
        
            updateAudioCDMetadata();
            break;

         case CDS_DATA_1:
         case CDS_DATA_2:
         case CDS_XA_2_1:
         case CDS_XA_2_2:
         case CDS_MIXED:
         
            //
            //  Don't do anything yet
            //

            break;
         
         default:
         
            warning("unknown response for cdrom disc status query");
    }
}

void DeviceWatcher::updateAudioCDMetadata()
{
    //
    //  Ok, dokey ... we got'em an audio CD
    //
    
    CdDecoder *cd_decoder = new CdDecoder(device_to_watch + "/1.cda", NULL, NULL, NULL);
    if(!cd_decoder)
    {
        warning(QString("cd decoder had troubles trying "
                        "to get metadata from \"%1\"")
                        .arg(device_to_watch));
    }

    int number_of_total_tracks = cd_decoder->getNumTracks();
    int number_of_audio_tracks = cd_decoder->getNumCDAudioTracks();
    
    if(number_of_audio_tracks < 1)
    {
        //
        //  Nothing to hear here
        //

        warning(QString("found no audio tracks on disc recently "
                        "inserted in %1, but I could have sworn "
                        "it was an audio cd")
                        .arg(device_to_watch));
        delete cd_decoder;
        return;
    }

    
    //
    //  Build some containers that will get passed to the metadata server
    //    

    QIntDict<Metadata>  *new_metadata = new QIntDict<Metadata>;

    QValueList<int>     metadata_additions;
    QValueList<int>     metadata_deletions;
    
        
    QString songlist = ""; 
    QString playlist_name = "";
    for(int i = 0; i < number_of_total_tracks; i++)
    {
        AudioMetadata *new_track = cd_decoder->getMetadata(i+1);
        if(new_track)
        {
            new_track->setId(i+1);
            new_track->setDbId(i+1);
            new_metadata->insert(new_track->getId(), new_track);
            metadata_additions.push_back(new_track->getId());
            if(songlist.length() < 1)
            {
                songlist.append(QString("%1").arg(new_track->getId()));
                playlist_name = QString("CD ~ %1 ~ %2")
                                .arg(new_track->getArtist())
                                .arg(new_track->getAlbum());
            }
            else
            {
                songlist.append(QString(",%1").arg(new_track->getId()));
            }
        }
    }
    
    //
    //  We are done with cd "decoder"
    //

    delete cd_decoder;
    

    if(new_metadata->count() < 1)
    {
        warning("huh ... got no tracks back from cd decoder");
        delete new_metadata;
        return;
    }

    //
    //  Ok, we have some audio data. Need to ask the metadata server to
    //  allocate us a container.
    //
    
    current_metadata_container = 
        metadata_server->createContainer(playlist_name, MCCT_audio, MCLT_host);
    
    //
    //  It's an audio CD, so it's ripable
    //
    
    current_metadata_container->setRipable(true);

    //
    //  Set the right collection id (now that we have a collection id) for
    //  the tracks
    //
    
    QIntDictIterator<Metadata> it( *new_metadata ); 
    for ( ; it.current(); ++it )
    {
        it.current()->setCollectionId(current_metadata_container->getIdentifier());
    }

    //
    //  Build one playlist that is all the tracks on this CD
    //

    QIntDict<Playlist>  *new_playlists = new QIntDict<Playlist>;
    QValueList<int>     playlist_additions;
    QValueList<int>     playlist_deletions;

    Playlist *new_playlist = new Playlist(
                                            current_metadata_container->getIdentifier(),
                                            playlist_name,
                                            songlist,
                                            1
                                         );
    new_playlist->setRipable(true);
    new_playlists->insert(1, new_playlist);
    playlist_additions.push_back(1);

    new_playlist->mapDatabaseToId(
                                    new_metadata,
                                    new_playlist->getDbList(),
                                    new_playlist->getListPtr(),
                                    new_playlists,
                                    0
                                 );

    //
    //  Stuff the new metadata and playlists into the metadata server
    //

    metadata_server->doAtomicDataSwap(  
                                        current_metadata_container, 
                                        new_metadata, 
                                        metadata_additions,
                                        metadata_deletions,
                                        new_playlists,
                                        playlist_additions,
                                        playlist_deletions
                                     );
    
    //
    //  Send out an even to let everyone know the metadata has changed
    //          

    MetadataChangeEvent *mce = new MetadataChangeEvent(current_metadata_container->getIdentifier());
    QApplication::postEvent(my_mfd, mce);    

}

void DeviceWatcher::log(const QString &log_message, int verbosity)
{
    if(parent)
    {
        parent->log(log_message, verbosity);
    }
}

void DeviceWatcher::warning(const QString &warn_message)
{
    if(parent)
    {
        parent->warning(warn_message);
    }
}

DeviceWatcher::~DeviceWatcher()
{
}
