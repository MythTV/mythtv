/*
	servicelister.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Small object that maintains a list of all services (eg. http, daap,
	etc.) that are available
	
*/

#include "servicelister.h"
#include "mfd_events.h"
#include "mfd.h"

ServiceLister::ServiceLister(MFD *owner)
{
    if(!owner)
    {
        cerr << "service lister created without mfd parent, pending doom..."
             << endl;
    }
    parent = owner;
}

void ServiceLister::lockBroadcastList()
{
    broadcast_list_mutex.lock();
}

void ServiceLister::unlockBroadcastList()
{
    broadcast_list_mutex.unlock();
}

void ServiceLister::lockDiscoveredList()
{
    discovered_list_mutex.lock();
}

void ServiceLister::unlockDiscoveredList()
{
    discovered_list_mutex.unlock();
}

void ServiceLister::handleServiceEvent(ServiceEvent *se)
{

    if (se->pleaseBroadcast())
    {
        if (se->newlyCreated())
        {
            addBroadcastService(se->getService());
        }
        else
        {
            removeBroadcastService(se->getService());
        }
    }
    else
    {
        if(se->newlyCreated())
        {
            addDiscoveredService(se->getService());
        }
        else
        {
            removeDiscoveredService(se->getService());
        }
    }
}

void ServiceLister::addBroadcastService(Service *service)
{
    //
    //  If this service does not have a hostname, we cannot hope to
    //  broadcast it
    //
    
    if (!service->hostKnown())
    {
        warning(QString("asked to broadcast a service called "
                        "%1 without a hostname")
                        .arg(service->getName()));
        return;
    }

    broadcast_list_mutex.lock();
    
        //
        //  Check that this service does not already exist
        //
    
        if ( findBroadcastService(  service->getName()) )
        {
            warning(QString("asked to add a broadcast service that "
                            "already exists: %1 (%2) %3:%4")
                           .arg(service->getName())
                           .arg(service->getType())
                           .arg(service->getHostname())
                           .arg(service->getPort())); 

            broadcast_list_mutex.unlock();
            return;
        }

        //
        //  Add it to the list
        //
        
        broadcast_service_list.append(*service);
        
        log(QString("added \"%1\" to the broadcast service list (total now %2)")
            .arg(service->getName())
            .arg(broadcast_service_list.count()), 7);

    broadcast_list_mutex.unlock();
}

void ServiceLister::removeBroadcastService(Service *service)
{
    //
    //  If this service does not have a hostname, we cannot hope to
    //  remove it
    //
    
    if (!service->hostKnown())
    {
        warning(QString("asked to remove a broadcast service called "
                        "%1 without a hostname")
                        .arg(service->getName()));
        return;
    }

    broadcast_list_mutex.lock();
    
        //
        //  Find it
        //
    
        Service *to_remove = findBroadcastService( service->getName() );

        if( !to_remove)
        {
            warning(QString("asked to remove a broadcast service that "
                            "does not exists: %1 (%2) %3:%4")
                           .arg(service->getName())
                           .arg(service->getType())
                           .arg(service->getHostname())
                           .arg(service->getPort())); 
            broadcast_list_mutex.unlock();
            return;
        }

        //
        //  Remove it from the list
        //
        
        QString removed_name = to_remove->getName();
        if (broadcast_service_list.remove(*to_remove))
        {
            log(QString("removed \"%1\" from the broadcast service "
                        "list (total now %2)")
                .arg(removed_name)
                .arg(broadcast_service_list.count()), 7);   
        }
        else
        {
            warning(QString("unable to remove service \"%1\" "
                            "from the broadcast service list")
                    .arg(removed_name));
        }

    broadcast_list_mutex.unlock();
}

Service* ServiceLister::findBroadcastService( const QString &name )
{
    //
    //  The broadcast list mutex should be locked before calling this
    //  function
    //
    
    if ( broadcast_list_mutex.tryLock() )
    {
        warning("findBroadcastService() called without list mutex on");
        broadcast_list_mutex.unlock();
    }
    
    ServiceList::iterator it;
    for ( it  = broadcast_service_list.begin(); 
          it != broadcast_service_list.end(); 
          ++it )
    {
        if( (*it).getName() == name )
        {
            return &(*it);
        }
    }
    
    return NULL;
}

Service* ServiceLister::findDiscoveredService( const QString &name )
{
    //
    //  The discovered list mutex should be locked before calling this
    //  function
    //
    
    if ( discovered_list_mutex.tryLock() )
    {
        warning("findDiscoveredService() called without list mutex on");
        discovered_list_mutex.unlock();
    }
    
    ServiceList::iterator it;
    for ( it  = discovered_service_list.begin(); 
          it != discovered_service_list.end(); 
          ++it )
    {
        if( (*it).getName() == name )
        {
            return &(*it);
        }
    }
    
    return NULL;
}

QValueList<Service>* ServiceLister::getBroadcastList()
{
    //
    //  The broadcast list mutex should be locked before calling this
    //  function
    //
    
    if ( broadcast_list_mutex.tryLock() )
    {
        warning("getBroadcastList() called without list mutex on");
        broadcast_list_mutex.unlock();
    }

    return &broadcast_service_list;
}

QValueList<Service>* ServiceLister::getDiscoveredList()
{
    //
    //  The discovered list mutex should be locked before calling this
    //  function
    //
    
    if ( discovered_list_mutex.tryLock() )
    {
        warning("getDiscoveredList() called without list mutex on");
        discovered_list_mutex.unlock();
    }

    return &discovered_service_list;
}

void ServiceLister::addDiscoveredService(Service *service)
{
    //
    //  Add this to the list 
    //
    
    discovered_list_mutex.lock();
    
        //
        //  Check that this service does not already exist
        //
    
        if ( findDiscoveredService(  service->getName()) )
        {
            warning(QString("asked to add a discovered service that "
                            "already exists: %1 (%2) %3:%4")
                           .arg(service->getName())
                           .arg(service->getType())
                           .arg(service->getHostname())
                           .arg(service->getPort())); 

            discovered_list_mutex.unlock();
            return;
        }

        //
        //  Add it to the list
        //
        
        discovered_service_list.append(*service);

        log(QString("added \"%1\" to the discovered service list (total now %2)")
            .arg(service->getName())
            .arg(discovered_service_list.count()), 7);


    discovered_list_mutex.unlock();
    
    //
    //  Ask the core mfd object to tell all mdcap clients that this service
    //  was discovered
    //
    
    parent->sendMessage(service->getFormalDescription(true));
}

void ServiceLister::removeDiscoveredService(Service *service)
{
    //
    //  Remove it from the list
    //
    
    discovered_list_mutex.lock();
    
        //
        //  Find it
        //
    
        Service *to_remove = findDiscoveredService( service->getName() );

        if( !to_remove)
        {
            warning(QString("asked to remove a discovered service that "
                            "does not exists: %1 (%2) %3:%4")
                           .arg(service->getName())
                           .arg(service->getType())
                           .arg(service->getHostname())
                           .arg(service->getPort())); 
            discovered_list_mutex.unlock();
            return;
        }

        //
        //  Remove it from the list
        //
        
        QString removed_name = to_remove->getName();
        if (discovered_service_list.remove(*to_remove))
        {
            log(QString("removed \"%1\" from the discovered service "
                        "list (total now %2)")
                .arg(removed_name)
                .arg(discovered_service_list.count()), 7);   
        }
        else
        {
            warning(QString("unable to remove service \"%1\" "
                            "from the discovered service list")
                    .arg(removed_name));
        }

    discovered_list_mutex.unlock();

    //
    //  Ask the core mfd object to tell all mdcap clients that this service
    //  has gone away
    //
    
    parent->sendMessage(service->getFormalDescription(false));
}

void ServiceLister::log(const QString &log_text, int verbosity)
{
    QString text = log_text;
    text.prepend("mfd's service lister: ");
    parent->log(text, verbosity);
}

void ServiceLister::warning(const QString &warning_text)
{
    QString text = warning_text;
    text.prepend("mfd's service lister: ");
    parent->warning(text);
}


ServiceLister::~ServiceLister()
{
}




