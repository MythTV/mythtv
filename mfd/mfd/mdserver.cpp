/*
	mdserver.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Implementation of a threaded object that opens a server port and
	explains the state of the metadata to any client that asks. It _also_
	holds all the metadatacontainers, so it is _the_ place to ask about what
	content is available

*/

#include <iostream>
using namespace std;

#include "settings.h"

#include "mdserver.h"
#include "../mfdlib/mfd_events.h"

MetadataServer::MetadataServer(MFD* owner, int port)
               :MFDServicePlugin(owner, -1, port, "metadata server")
{
    metadata_containers = new QPtrList<MetadataContainer>;
    metadata_audio_generation = 4;  //  don't ask
    container_identifier = 0;
}

void MetadataServer::run()
{
    //
    //  Advertise ourselves to the world (via zeroconfig) as an mdcap (Myth
    //  Digital Content Access Protocol) service
    //

    QString local_hostname = mfdContext->getHostName();

    ServiceEvent *se = new ServiceEvent(QString("services add mdcap %1 Myth Metadata Services on %2")
                                       .arg(port_number)
                                       .arg(local_hostname));
    QApplication::postEvent(parent, se);


    //
    //  Init our sockets (note, these are low-level Socket Devices, which
    //  are thread safe)
    //
    
    if( !initServerSocket())
    {
        fatal("metadata server could not initialize its core server socket");
        return;
    }
    
    while(keep_going)
    {
        //
        //  Update the status of our sockets.
        //
        
        updateSockets();
        waitForSomethingToHappen();
    }
}

void MetadataServer::doSomething(const QStringList &tokens, int socket_identifier)
{
    bool ok = false;

    if(tokens.count() < 1)
    {
        ok = false;
    }
    else if(tokens[0] == "check" && tokens.count() == 1)
    {
        ok = true;
        //checkMetadata();
    }

    if(!ok)
    {
        warning(QString("did not understand these tokens: %1").arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
    }
}

void MetadataServer::lockMetadata()
{
    metadata_mutex.lock();
}

void MetadataServer::unlockMetadata()
{
    metadata_mutex.unlock();
}

void MetadataServer::lockPlaylists()
{
    playlists_mutex.lock();
}

void MetadataServer::unlockPlaylists()
{
    playlists_mutex.unlock();
}

uint MetadataServer::getMetadataAudioGeneration()
{
    uint return_value;
    metadata_audio_generation_mutex.lock();
        return_value = metadata_audio_generation;
    metadata_audio_generation_mutex.unlock();
    return return_value;
}

uint MetadataServer::getAllAudioMetadataCount()
{
    //
    //  Iterate over all Audio collections and count the items
    //

    uint return_value;

    lockMetadata();

        return_value = 0;
        MetadataContainer * a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container->isAudio())
            {
                return_value += a_container->getMetadataCount();
            }
        }
    
    unlockMetadata();
    return return_value;
}

uint MetadataServer::getAllAudioPlaylistCount()
{
    //
    //  Iterate over all Audio collections and count the playlists
    //

    uint return_value;

    lockPlaylists();

        return_value = 0;
        MetadataContainer * a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container->isAudio())
            {
                return_value += a_container->getPlaylistCount();
            }
        }
    
    unlockPlaylists();
    return return_value;

}

Metadata* MetadataServer::getMetadataByUniversalId(uint universal_id)
{
    if(universal_id <= METADATA_UNIVERSAL_ID_DIVIDER)
    {
        warning("something asked for metadata with "
                "universal id that is too small");
        return NULL;
    }
    
    int collection_id = universal_id / METADATA_UNIVERSAL_ID_DIVIDER;
    int item_id = universal_id % METADATA_UNIVERSAL_ID_DIVIDER;
    
    //
    //  Find the collection
    //
    
    Metadata *return_value = NULL;
    
    lockMetadata();

        MetadataContainer * a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container->getIdentifier() == collection_id)
            {
                return_value = a_container->getMetadata(item_id);
                break; 
            }
        }

    unlockMetadata();        
    
    return return_value;
}

Playlist* MetadataServer::getPlaylistByUniversalId(uint universal_id)
{
    if(universal_id <= METADATA_UNIVERSAL_ID_DIVIDER)
    {
        warning("something asked for playlist with "
                "universal id that is too small");
        return NULL;
    }
    
    int collection_id = universal_id / METADATA_UNIVERSAL_ID_DIVIDER;
    int playlist_id = universal_id % METADATA_UNIVERSAL_ID_DIVIDER;
    
    //
    //  Find the collection
    //
    
    Playlist *return_value = NULL;
    
    lockMetadata();

        MetadataContainer * a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container->getIdentifier() == collection_id)
            {
                return_value = a_container->getPlaylist(playlist_id);
                break; 
            }
        }
    unlockMetadata();        
    
    return return_value;
}

MetadataContainer* MetadataServer::createContainer(
                                                    MetadataCollectionContentType content_type,
                                                    MetadataCollectionLocationType location_type
                                                  )
{
    MetadataContainer *return_value;
    lockMetadata();
        return_value = new MetadataContainer(parent, bumpContainerId(), content_type, location_type);
        metadata_containers->append(return_value);
    unlockMetadata();
    
    return return_value;
}                                                                                                                                                        

int MetadataServer::bumpContainerId()
{
    int return_value;

    container_identifier_mutex.lock();
        ++container_identifier;
        return_value = container_identifier;
    container_identifier_mutex.unlock();
    
    return return_value;
}

void MetadataServer::doAtomicDataSwap(
                                        MetadataContainer *which_one,
                                        QIntDict<Metadata>* new_metadata,
                                        QValueList<int> metadata_additions,
                                        QValueList<int> metadata_deletions,
                                        QIntDict<Playlist>* new_playlists
                                     )
{
    //
    //  Lock the metadata, find the right container, and swap out its data.
    //  The idea is that a plugin can take as long as it wants to build a
    //  new metadata collection, but this call (which has the needed locks)
    //  is very quick. Thus the name.
    //

    lockMetadata();

        MetadataContainer *target = NULL;
        MetadataContainer *a_container;
        for (
                a_container = metadata_containers->first(); 
                a_container; 
                a_container = metadata_containers->next()
            )
        {
            if(a_container == which_one)
            {
                target = a_container;
                break; 
            }
        }
        
        if(target)
        {

            target->dataSwap(
                                new_metadata, 
                                metadata_additions,
                                metadata_deletions,
                                new_playlists
                            );
            if(target->isAudio())
            {
                metadata_audio_generation_mutex.lock();
                    ++metadata_audio_generation;
                metadata_audio_generation_mutex.unlock();
            }
        }
        else
        {
            //  Crap!
        }
        
    unlockMetadata();
}


MetadataServer::~MetadataServer()
{
    if(metadata_containers)
    {
        delete metadata_containers;
    }
}
