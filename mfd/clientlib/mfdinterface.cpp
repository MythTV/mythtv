/*
	mfdinterface.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <iostream>
using namespace std;

#include "mfdinterface.h"
#include "mfdinstance.h"
#include "discoverythread.h"
#include "events.h"

MfdInterface::MfdInterface()
{
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
}

void MfdInterface::playAudio(int which_mfd, int container, int type, int which_id, int index)
{
    //
    //  Find the instance, ask it to play something
    //  

    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if(an_mfd->getId() == which_mfd)
        {
            an_mfd->playAudio(container, type, which_id, index);
            break;
        }
    }
}

void MfdInterface::stopAudio(int which_mfd)
{
    //
    //  Find the instance, ask it to play something
    //  

    for(
        MfdInstance *an_mfd = mfd_instances->first();
        an_mfd;
        an_mfd = mfd_instances->next()
       )
    {
        if(an_mfd->getId() == which_mfd)
        {
            an_mfd->stopAudio();
            break;
        }
    }
}



void MfdInterface::customEvent(QCustomEvent *ce)
{
    //
    //  This is pretty critical. It receives events from the mfd instances
    //  (and their service client interfaces) and emits Qt signals to let
    //  the client application code know what is going on
    //

    if(ce->type() == MFD_CLIENTLIB_EVENT_DISCOVERY)
    {
        //
        //  It's an mfd discovery event
        //

        MfdDiscoveryEvent *de = (MfdDiscoveryEvent*)ce;
        if(de->getLostOrFound())
        {
        
            //
            //  Make sure we can't find this one already
            //
            
            MfdInstance *already_mfd = findMfd(
                                                de->getHost(), 
                                                de->getAddress(), 
                                                de->getPort()
                                              );
            if(already_mfd)
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
            if(mfd_to_kill)
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
    else if(ce->type() == MFD_CLIENTLIB_EVENT_AUDIOPAUSE)
    {
        MfdAudioPausedEvent *ape = (MfdAudioPausedEvent*)ce;
        emit audioPaused(ape->getMfd(), ape->getPaused());
    }
    else if(ce->type() == MFD_CLIENTLIB_EVENT_AUDIOSTOP)
    {
        MfdAudioStoppedEvent *ase = (MfdAudioStoppedEvent*)ce;
        emit audioStopped(ase->getMfd());
    }
    else if(ce->type() == MFD_CLIENTLIB_EVENT_AUDIOPLAY)
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
    else if(ce->type() == MFD_CLIENTLIB_EVENT_METADATA)
    {
        MfdMetadataChangedEvent *mce = (MfdMetadataChangedEvent*)ce;
        swapMetadataTree(mce->getMfd(), mce->getNewTree());
    }
    else
    {
        cerr << "mfdinterface is getting CustomEvent's it does not understand" 
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
        if(
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

void MfdInterface::swapMetadataTree(int which_mfd, GenericTree *new_tree)
{
    //
    //  One of those darn metadata clients running in a sub thread somewhere
    //  has handed me a new metadata tree
    //
    
    GenericTree *old_tree = NULL;
    
    //
    //  See if we already have one for this mfd.
    //
    
    old_tree = metadata_trees.find(which_mfd);
    
    if(old_tree)
    {
        cout << "swapped in new tree for old" << endl;
        metadata_trees.remove(which_mfd);
    }
    else
    {
        cout << "no old tree, sticking in new one" << endl;
    }
    
    emit metadataChanged(which_mfd, new_tree);
    
    //
    //  Keep a reference so we can delete it when the next swap comes in
    //
    
    metadata_trees.insert(which_mfd, new_tree);
    
    //
    //  Delete the old one
    //
    
    delete old_tree;
}
                                                                                            

MfdInterface::~MfdInterface()
{
    //
    //  Shut down and delete the discovery thread
    //
    
    if(discovery_thread)
    {
        discovery_thread->stop();
        discovery_thread->wait();
        delete discovery_thread;
    }

    if(mfd_instances)
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
}
