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
}

void MfdInterface::customEvent(QCustomEvent *ce)
{
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
                                                        de->getHost(),
                                                        de->getAddress(),
                                                        de->getPort()
                                                      );
                new_mfd->start();
                mfd_instances->append(new_mfd);
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
            }

        }
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
