/*
	mfdinterface.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <iostream>
using namespace std;

#include "mfdinterface.h"
#include "mfdinstance.h"
#include "discoverythread.h"
#include "playlistchecker.h"
#include "events.h"
#include "speakertracker.h"

MfdInterface::MfdInterface(int client_screen_width, int client_screen_height)
{

    //
    //  Store information about the GUI client that has loaded us as a library
    //
    
    client_width = client_screen_width;
    client_height = client_screen_height;
    
    //
    //  Create a thread which just sits around looking for mfd's to appear or dissappear
    //

    discovery_thread = new DiscoveryThread(this);
    
    //
    //  run the discovery thread
    //
    
    discovery_thread->start();
    
    //
    //  Create our (empty) collection of MFD instances
    //
    
    mfd_instances = new QPtrList<MfdInstance>;
    mfd_instances->setAutoDelete(true);
    mfd_id_counter = 0;
    
    //
    //  Create a thread that the linked client can use to properly check
    //  mark
    //
    
    playlist_checker = new PlaylistChecker(this);
    
    //
    //  Set it running
    //
    
    playlist_checker->start();
}

void MfdInterface::playAudio(int which_mfd, int container, int type, int which_id, int index)
{
    //
    //  Find the instance, hand it a play command
    //  

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (an_mfd->getId() == which_mfd)
        {
            an_mfd->addPendingCommand(
                                        QStringList::split(" ", QString("audio play %1 %2 %3 %4")
                                                .arg(container)
                                                .arg(type)
                                                .arg(which_id)
                                                .arg(index))
                                     );
            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for a playAudio() request"
             << endl;
    }
}

void MfdInterface::stopAudio(int which_mfd)
{
    //
    //  Find the instance, hand it a stop command
    //  

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (an_mfd->getId() == which_mfd)
        {
            an_mfd->addPendingCommand(
                                        QStringList::split(" ", QString("audio stop"))
                                     );
            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for a stopAudio() request"
             << endl;
    }
}


void MfdInterface::pauseAudio(int which_mfd, bool on_or_off)
{
    //
    //  Find the instance, hand it a pause command
    //  

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (an_mfd->getId() == which_mfd)
        {
            if (on_or_off)
            {
                an_mfd->addPendingCommand(
                                            QStringList::split(" ", QString("audio pause on"))
                                         );
            }
            else
            {
                an_mfd->addPendingCommand(
                                            QStringList::split(" ", QString("audio pause off"))
                                         );
            }

            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for a pauseAudio() request"
             << endl;
    }
}

void MfdInterface::seekAudio(int which_mfd, int how_much)
{
    //
    //  Find the instance, hand it a seek command
    //  

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (an_mfd->getId() == which_mfd)
        {
            an_mfd->addPendingCommand(
                                        QStringList::split(" ", QString("audio seek %1")
                                                                        .arg(how_much))
                                     );

            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for a seekAudio() request"
             << endl;
    }
}

void MfdInterface::nextAudio(int which_mfd)
{
    //
    //  Find the instance, hand it a next command
    //  

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (an_mfd->getId() == which_mfd)
        {
            an_mfd->addPendingCommand(
                                        QStringList::split(" ", QString("audio next"))
                                     );

            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for a nextAudio() request"
             << endl;
    }
}

void MfdInterface::prevAudio(int which_mfd)
{
    //
    //  Find the instance, hand it a prev command
    //  

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (an_mfd->getId() == which_mfd)
        {
            an_mfd->addPendingCommand(
                                        QStringList::split(" ", QString("audio prev"))
                                     );

            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for a prevAudio() request"
             << endl;
    }
}

void MfdInterface::askForStatus(int which_mfd)
{
    //
    //  Find the instance, hand it a status command
    //  

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (an_mfd->getId() == which_mfd)
        {
            an_mfd->addPendingCommand(
                                        QStringList::split(" ", QString("audio status"))
                                     );

            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for an askForStatus() request"
             << endl;
    }
}

void MfdInterface::toggleSpeakers(int which_mfd, int which_speaker, bool on_or_off)
{
    //
    //  Find the instance, toggle it's speaker
    //  

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {

        if (an_mfd->getId() == which_mfd)
        {
            if(on_or_off)
            {
                an_mfd->addPendingCommand(
                                            QStringList::split(" ", 
                                            QString("audio speakeruse %1 yes")
                                            .arg(which_speaker))
                                         );
            }
            else
            {
                an_mfd->addPendingCommand(
                                            QStringList::split(" ", 
                                            QString("audio speakeruse %1 no")
                                            .arg(which_speaker))
                                         );
            }

            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for a toggleSpeaker() request"
             << endl;
    }
}

void MfdInterface::commitListEdits(
                                    int which_mfd, 
                                    UIListGenericTree *playlist_tree,
                                    bool new_playlist,
                                    QString playlist_name
                                  )
{
    //
    //  This has to happen here in the main execution thread, as
    //  playlist_tree will probably get deleted very shortly after we return
    //

    bool found_it = false;
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (an_mfd->getId() == which_mfd)
        {
            an_mfd->commitListEdits(
                                    playlist_tree,
                                    new_playlist,
                                    playlist_name
                                   );
            found_it = true;
            break;
        }
    }
    
    if (!found_it)
    {
        cerr << "mfdinterface.o: could not find an mfd "
             << "for a commitEdits() request"
             << endl;
    }
}

void MfdInterface::startPlaylistCheck(
                                        MfdContentCollection *mfd_collection,
                                        UIListGenericTree *playlist, 
                                        UIListGenericTree *content,
                                        QIntDict<bool> *playlist_additions,
                                        QIntDict<bool> *playlist_deletions
                                     )
{
    if (playlist_checker)
    {
        playlist_checker->startChecking(
                                        mfd_collection, 
                                        playlist, 
                                        content,
                                        playlist_additions,
                                        playlist_deletions
                                       );
    }
    else
    {
        cerr << "mfdinterface.o: asked to startPlaylistCheck(), but I don't "
             << "have a PlaylistChecker thread."
             << endl;
    }
}


void MfdInterface::stopPlaylistCheck()
{
    if (playlist_checker)
    {
        playlist_checker->stopChecking();
    }
    else
    {
        cerr << "mfdinterface.o: asked to stopPlaylistCheck(), but I don't "
             << "have a PlaylistChecker thread."
             << endl;
    }
}


void MfdInterface::customEvent(QCustomEvent *ce)
{
    //
    //  This is pretty critical. It receives events from the mfd instances
    //  (and their service client interfaces) and emits Qt signals to let
    //  the client application code know what is going on
    //

    if (ce->type() == MFD_CLIENTLIB_EVENT_DISCOVERY)
    {
        //
        //  It's an mfd discovery event
        //

        MfdDiscoveryEvent *de = (MfdDiscoveryEvent*)ce;
        if (de->getLostOrFound())
        {
        
            //
            //  Make sure we can't find this one already
            //
            
            MfdInstance *already_mfd = findMfd(
                                                de->getHost(), 
                                                de->getAddress(), 
                                                de->getPort()
                                              );
            if (already_mfd)
            {
                cerr << "was told there is a new mfd, "
                     << "but it's the same as one that already "
                     << "exists"
                     << endl;  
            }
            else
            {
                MfdInstance *new_mfd = new MfdInstance(
                                                        bumpMfdId(),
                                                        this,
                                                        de->getName(),
                                                        de->getHost(),
                                                        de->getAddress(),
                                                        de->getPort()
                                                      );
                new_mfd->start();
                mfd_instances->append(new_mfd);
                emit mfdDiscovery(
                                    new_mfd->getId(), 
                                    new_mfd->getName().section(".", 0, 0), 
                                    new_mfd->getHostname(),
                                    true
                                 );
            
            }
                                               
        }
        else
        {
            MfdInstance *mfd_to_kill = findMfd(
                                                de->getHost(), 
                                                de->getAddress(), 
                                                de->getPort()
                                              );
            if (mfd_to_kill)
            {
                mfd_to_kill->stop();
                mfd_to_kill->wait();
                mfd_instances->remove(mfd_to_kill);
                emit mfdDiscovery(
                                    mfd_to_kill->getId(), 
                                    mfd_to_kill->getName().section(".", 0, 0), 
                                    mfd_to_kill->getHostname(),
                                    false
                                 );
            }
            else
            {
                cerr << "told to kill an mfd that I have no record of "
                     << endl;
            }
        }
    }
    else if (ce->type() == MFD_CLIENTLIB_EVENT_AUDIOPAUSE)
    {
        MfdAudioPausedEvent *ape = (MfdAudioPausedEvent*)ce;
        emit audioPaused(ape->getMfd(), ape->getPaused());
    }
    else if (ce->type() == MFD_CLIENTLIB_EVENT_AUDIOSTOP)
    {
        MfdAudioStoppedEvent *ase = (MfdAudioStoppedEvent*)ce;
        emit audioStopped(ase->getMfd());
    }
    else if (ce->type() == MFD_CLIENTLIB_EVENT_AUDIOPLAY)
    {
        MfdAudioPlayingEvent *ape = (MfdAudioPlayingEvent*)ce;
        emit audioPlaying(
                            ape->getMfd(),
                            ape->getPlaylist(),
                            ape->getPlaylistIndex(),
                            ape->getCollectionId(),
                            ape->getMetadataId(),
                            ape->getSeconds(),
                            ape->getChannels(),
                            ape->getBitrate(),
                            ape->getSampleFrequency()
                         );
    }
    else if (ce->type() == MFD_CLIENTLIB_EVENT_METADATA)
    {
        MfdMetadataChangedEvent *mce = (MfdMetadataChangedEvent*)ce;
        swapMetadata(mce->getMfd(), mce->getNewCollection());
    }
    else if (ce->type() == MFD_CLIENTLIB_EVENT_AUDIOPLUGIN_EXISTS)
    {
        MfdAudioPluginExistsEvent *apee = (MfdAudioPluginExistsEvent*)ce;
        emit audioPluginDiscovery(apee->getMfd());
    }
    else if (ce->type() == MFD_CLIENTLIB_EVENT_PLAYLIST_CHECKED)
    {
        emit playlistCheckDone();
    }
    else if (ce->type() == MFD_CLIENTLIB_EVENT_SPEAKER_LIST)
    {
        MfdSpeakerListEvent *sle = (MfdSpeakerListEvent*)ce;
        QPtrList<SpeakerTracker>* speakers = sle->getSpeakerList();
        if(speakers)
        {
            emit speakerList(sle->getMfd(), speakers);
        }
    }
    
    else
    {
        cerr << "mfdinterface is getting CustomEvents it does not understand" 
             << endl;
    }
}

MfdInstance* MfdInterface::findMfd(
                                    const QString &a_host,
                                    const QString &an_ip_address,
                                    int a_port
                                  )
{
    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if (
            an_mfd->getHostname() == a_host &&
            an_mfd->getAddress() == an_ip_address &&
            an_mfd->getPort() == a_port
          )
        {
            return an_mfd;
        }
    }
    return NULL;
}                                  

void MfdInterface::swapMetadata(int which_mfd, MfdContentCollection *new_collection)
{
    //
    //  One of those darn metadata clients running in a sub thread somewhere
    //  has handed me a new metadata tree
    //
    
    MfdContentCollection *old_collection = NULL;
    
    //
    //  See if we already have one for this mfd.
    //
    

    old_collection = mfd_collections.find(which_mfd);
    
    if (old_collection)
    {
        mfd_collections.remove(which_mfd);
    }
    else
    {
        //cout << "no old collection, sticking in new one" << endl;
    }
    
    emit metadataChanged(which_mfd, new_collection);
    
    //
    //  Keep a reference so we can delete it when the next swap comes in
    //
    
    mfd_collections.insert(which_mfd, new_collection);
    
    //
    //  Delete the old one
    //
    
    if (old_collection)
    {
        delete old_collection;
        old_collection = NULL;
    }
}
                                                                                            

MfdInterface::~MfdInterface()
{
    //
    //  Shut down and delete the discovery thread
    //
    
    if (discovery_thread)
    {
        discovery_thread->stop();
        discovery_thread->wait();
        delete discovery_thread;
        discovery_thread= NULL;
    }

    if (mfd_instances)
    {
        //
        //  Shut them all down
        //

        for(
            MfdInstance *an_mfd = mfd_instances->first();
            an_mfd;
            an_mfd = mfd_instances->next()
           )
        {
            an_mfd->stop();
            an_mfd->wait();
        }

        //
        //  Delete them, then delete the list
        //

        mfd_instances->clear();
        delete mfd_instances;
        mfd_instances = NULL;
    }
    
    //
    //  Shutdown the playlist checker
    //
    
    if (playlist_checker)
    {
        playlist_checker->stop();
        playlist_checker->wait();
        delete playlist_checker;
        playlist_checker = NULL;
    }
    
}
