/*
	daapclient.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for a daap content monitor

*/

#include <qhostaddress.h>
#include <qregexp.h>

#include "daapclient.h"
#include "../../servicelister.h"

DaapClient::DaapClient(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 0, "daap client", false)
{
    daap_instances.setAutoDelete(true);
}

void DaapClient::run()
{

    while(keep_going)
    {

        //
        //  Check for service changes (i.e. any new daap servers out there?)
        //

        checkServiceChanges();

        int nfds = 0;
        fd_set readfds;

        FD_ZERO(&readfds);

        //
        //  Add the control pipe
        //
            
        FD_SET(u_shaped_pipe[0], &readfds);
        if(nfds <= u_shaped_pipe[0])
        {
            nfds = u_shaped_pipe[0] + 1;
        }
    
        timeout_mutex.lock();
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
        timeout_mutex.unlock();

        //
        //  Sit in select() until data arrives
        //
    
        int result = select(nfds, &readfds, NULL, NULL, &timeout);
        if(result < 0)
        {
            warning("got an error on select()");
        }
    
        //
        //  In case data came in on out u_shaped_pipe, clean it out
        //

        if(FD_ISSET(u_shaped_pipe[0], &readfds))
        {
            u_shaped_pipe_mutex.lock();
                char read_back[2049];
                read(u_shaped_pipe[0], read_back, 2048);
            u_shaped_pipe_mutex.unlock();
        }
    
        //
        //  Clean out any instances that have exited (because, for example,
        //  the daap server they were talking to went away)
        //
        
        DaapInstance *an_instance;
        QPtrListIterator<DaapInstance> iter( daap_instances );

        while ( (an_instance = iter.current()) != 0 )
        {
            ++iter;
            if(an_instance->finished())
            {
                daap_instances.remove(an_instance);
                log(QString("removed a daap client instance (total now %1)")
                    .arg(daap_instances.count()), 5);
                    
            }
        }
    }
    
    //
    //  Shut down all my connections to any and all daap servers
    //
    
    DaapInstance *an_instance;
    for ( an_instance = daap_instances.first(); an_instance; an_instance = daap_instances.next() )
    {
        an_instance->stop();
        an_instance->wakeUp();
        an_instance->wait();
    }
    
}

void DaapClient::handleServiceChange()
{
    //
    //  Something has changed in available services. I need to query the mfd
    //  and make sure I know about all daap servers
    //
    
    ServiceLister *service_lister = parent->getServiceLister();
    
    service_lister->lockDiscoveredList();

        //
        //  Go through my list of daap instances and make sure they still
        //  exist in the mfd's service list
        //

        DaapInstance *an_instance;
        QPtrListIterator<DaapInstance> iter( daap_instances );

        while ( (an_instance = iter.current()) != 0 )
        {
            ++iter;
            if( !service_lister->findDiscoveredService(an_instance->getName()) )
            {
                an_instance->stop();
                an_instance->wakeUp();
                break;
            }
        }

    
        //
        //  Go through services on the master list, add any that I don't
        //  already know about
        //
        
        typedef     QValueList<Service> ServiceList;
        ServiceList *discovered_list = service_lister->getDiscoveredList();
        

        ServiceList::iterator it;
        for ( it  = discovered_list->begin(); 
              it != discovered_list->end(); 
              ++it )
        {
            //
            //  We just try and add any that are not on this host, as the
            //  addDaapServer() method checks for dupes
            //
                
            if ( (*it).getType() == "daap")
            {
                if( (*it).getLocation() == SLT_LAN ||
                    (*it).getLocation() == SLT_NET )
                {
                    addDaapServer( (*it).getAddress(), (*it).getPort(), (*it).getName() );
                }
            }
                
        }

    service_lister->unlockDiscoveredList();
    
}

void DaapClient::addDaapServer(QString server_address, uint server_port, QString service_name)
{
    //
    //  If it's new, add it ... otherwise ignore it
    //


    bool already_have_it = false;
    DaapInstance *an_instance;
    for ( an_instance = daap_instances.first(); an_instance; an_instance = daap_instances.next() )
    {
        if(an_instance->isThisYou(service_name, server_address, server_port))
        {
            already_have_it = true;
            break;
        }
    }
    
    if(!already_have_it)
    {
        log(QString("daapclient will attempt to add service \"%1\" (%2:%3)")
            .arg(service_name)
            .arg(server_address)
            .arg(server_port), 10);
        
        DaapInstance *new_daap_instance = new DaapInstance(parent, this, server_address, server_port, service_name);
        new_daap_instance->start();
        daap_instances.append(new_daap_instance);
        log(QString("added a daap client instance (total now %1)")
                    .arg(daap_instances.count()), 5);
    }
}

void DaapClient::removeDaapServer(QString server_address, uint server_port, QString service_name)
{
    log(QString("daapclient will attempt to remove service \"%1\" (%2:%3)")
        .arg(service_name)
        .arg(server_address)
        .arg(server_port), 10);

    //
    //  Try and find it
    //
    
    DaapInstance *an_instance;
    for ( an_instance = daap_instances.first(); an_instance; an_instance = daap_instances.next() )
    {
        if(an_instance->isThisYou(service_name, server_address, server_port))
        {
            an_instance->stop();
            an_instance->wakeUp();
            break;
        }
    }
    

}

DaapClient::~DaapClient()
{
    daap_instances.clear();
}

