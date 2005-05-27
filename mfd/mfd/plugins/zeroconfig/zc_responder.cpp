/*
	zc_responder.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    !! This code is covered the the Apple Public Source License !!
    
    See ./apple/APPLE_LICENSE

*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>              

#include <qapplication.h>

#include "zc_supervisor.h"
#include "../../servicelister.h"

static  mDNS mDNSStorage_responder;                       // mDNS core uses this to store its globals
static  mDNS_PlatformSupport PlatformStorage_responder;   // Stores this platform's globals

ZeroConfigResponder *zero_config_responder = NULL;

/*
---------------------------------------------------------------------
*/

RegisteredService::RegisteredService(int id, QString name)
{
    unique_identifier = id;
    service_name = name;
    registered = false;
}

/*
---------------------------------------------------------------------
*/

extern "C" void RegistrationCallback(mDNS *const m, ServiceRecordSet *const thisRegistration, mStatus status)
{
    //
    //  mDNS Callback, jumps back into the ZeroConfigResponder object
    //
    
    zero_config_responder->handleRegistrationCallback(m, thisRegistration, status);    
}

/*
---------------------------------------------------------------------
*/


ZeroConfigResponder::ZeroConfigResponder(MFD *owner, int identifier)
                    :MFDServicePlugin(owner, identifier, 0, "zeroconfig responder", false)
{

    //
    //  Hold a pointer to the mfd so we can
    //  post events to it. Identifier lets
    //  the mfd know exactly who we are.
    //
    
    parent = owner;
    unique_identifier = identifier;
    service_identifier = 0;

    registered_services.setAutoDelete(true);

}


RegisteredService* ZeroConfigResponder::getServiceByName(QString name)
{
    QPtrListIterator<RegisteredService> iterator(registered_services);
    RegisteredService *a_service;
    while ( (a_service = iterator.current()) != 0 )
    {
        ++iterator;
        if(a_service->getName() == name)
        {
            return a_service;
        }
    }
    return NULL;
    
}

void ZeroConfigResponder::handleRegistrationCallback(
                                                        mDNS *const m,
                                                        ServiceRecordSet *const thisRegistration,
                                                        mStatus status
                                                    )
{
    if(m) {}    // -Wall -W
    
    //
    //  Figure out which of our RegisteredService Objects this callback is
    //  referring to
    //
    
    domainname name = thisRegistration->RR_SRV.resrec.name;
    char nameC[256];
    ConvertDomainNameToCString_unescaped(&name, nameC);
    QString service_name = nameC;
    service_name = service_name.section('.',0,0);

    RegisteredService *which_service = getServiceByName(service_name);
    
    if(!which_service)
    {
        warning(QString("could not find a registered service called %1")
                .arg(service_name));
        return;
    }

    switch (status) {

        case mStatus_NoError:
            
            log(QString("succesfully registered a service called \"%1\"")
                .arg(service_name), 1);
            which_service->setRegistered(true);
            break;

        case mStatus_NameConflict: 

            warning(QString("got a name conflict for \"%1\"").arg(service_name));
            break;

        case mStatus_MemFree:      
            
            registered_services.remove(which_service);
            log(QString("removed the following service from memory: \"%1\"")
                .arg(service_name), 2);
            break;

        default:
            warning(QString("got an unknown status from a callback on a service called %1")
                    .arg(service_name));                   
            break;
    }
}                                                                                                                                               

bool ZeroConfigResponder::formatServiceText(
                                            const char *serviceText,
                                            mDNSu8 *pStringList, 
                                            mDNSu16 *pStringListLen
                                           )
{
    //
    //  This is pretty much a straight rip from the Apple nMNSResponder.c
    //  code
    //

    mDNSBool result;
    size_t   serviceTextLen;
    
    // Note that parsing a C string into a PString list always 
    // expands the data by one character, so the following 
    // compare is ">=", not ">".  Here's the logic:
    //
    // #1 For a string with not ^A's, the PString length is one 
    //    greater than the C string length because we add a length 
    //    byte.
    // #2 For every regular (not ^A) character you add to the C 
    //    string, you add a regular character to the PString list.
    //    This does not affect the equivalence stated in #1.
    // #3 For every ^A you add to the C string, you add a length 
    //    byte to the PString list but you also eliminate the ^A, 
    //    which again does not affect the equivalence stated in #1.
    
    result = mDNStrue;
    serviceTextLen = strlen(serviceText);
    if (result && strlen(serviceText) >= sizeof(RDataBody))
    {
        result = mDNSfalse;
    }
    
    // Now break the string up into PStrings delimited by ^A.
    // We know the data will fit so we can ignore buffer overrun concerns. 
    // However, we still have to treat runs long than 255 characters as
    // an error.
    
    if (result)
    {
        int         lastPStringOffset;
        int         i;
        int         thisPStringLen;
        
        // This algorithm is a little tricky.  We start by copying 
        // the string directly into the output buffer, shifted up by 
        // one byte.  We then fill in the first byte with a ^A. 
        // We then walk backwards through the buffer and, for each 
        // ^A that we find, we replace it with the difference between 
        // its offset and the offset of the last ^A that we found
        // (ie lastPStringOffset).
        
        memcpy(&pStringList[1], serviceText, serviceTextLen);
        pStringList[0] = 1;
        lastPStringOffset = serviceTextLen + 1;
        for (i = serviceTextLen; i >= 0; i--)
        {
            if ( pStringList[i] == 1 ) 
            {
                thisPStringLen = (lastPStringOffset - i - 1);
                if(thisPStringLen < 0)
                {
                    warning("failed in parsing a service text ... bad things may happen soon");
                }
                if (thisPStringLen > 255) 
                {
                    result = mDNSfalse;
                    warning(": each component of a service text record must be 255 characters or less");
                    break;
                } 
                else 
                {
                    pStringList[i]    = thisPStringLen;
                    lastPStringOffset = i;
                }
            }
        }
        
        *pStringListLen = serviceTextLen + 1;
    }

    return result;
}



bool ZeroConfigResponder::registerService(
                                            const QString& service_name, 
                                            const QString& service_type, 
                                            uint  port_number,
                                            const QString& service_domain = "local.",
                                            const QString& service_text = ""
                                         )
{

    //
    //  First, check that this name is not already in use
    //
    
    RegisteredService *checker = getServiceByName(service_name);
    if(checker)
    {
        warning(QString("cannot register another service with name of %1")
                .arg(service_name));
        return false;
    }    

    mStatus             bstatus;
    mDNSOpaque16        port;
    domainlabel         name;
    domainname          type;
    domainname          domain;
    
    //
    //  Pack the service text into the correct format
    //
    
    mDNSu8      f_service_text[sizeof(RDataBody)];
    mDNSu16     f_service_text_length;


    if(!formatServiceText(service_text.ascii(), f_service_text, &f_service_text_length))
    {
        warning(QString("could not register a service called \"%1\" "
                        "because the service text was bad: %2")
                .arg(service_name)
                .arg(service_text));
        return false;
    }
    
    //
    //  Check through our parochial list of service types
    //
    
    QString strict_service_type = "";
    
    if(service_type == "mfdp")
    {
        strict_service_type = "_mfdp._tcp.";
    }
    else if(service_type == "macp")
    {
        strict_service_type = "_macp._tcp.";
    }
    else if(service_type == "daap")
    {
        strict_service_type = "_daap._tcp.";
    }
    else if(service_type == "mdcap")
    {
        strict_service_type = "_mdcap._tcp.";
    }
    else if(service_type == "http")
    {
        strict_service_type = "_http._tcp.";
    }
    else if(service_type == "maop")
    {
        strict_service_type = "_maop._tcp.";
    }
    else if(service_type == "rtsp")
    {
        strict_service_type = "_rtsp._tcp.";
    }
    else if(service_type == "mtcp")
    {
        strict_service_type = "_mtcp._tcp.";
    }

    if(strict_service_type.length() < 1)
    {
        warning(QString("could not register a service called \"%1\" "
                        "because it does not recognize a service type called \"%2\"")
                .arg(service_name)
                .arg(service_type));
                return false;
    }

    RegisteredService *new_service = new RegisteredService(bumpIdentifiers(), service_name);
    
    MakeDomainLabelFromLiteralString(&name,  service_name);
    MakeDomainNameFromDNSNameString(&type, strict_service_type);
    MakeDomainNameFromDNSNameString(&domain, service_domain);
    port.b[0] = (port_number >> 8) & 0x0FF;
    port.b[1] = (port_number >> 0) & 0x0FF;;
    bstatus = mDNS_RegisterService(&mDNSStorage_responder, new_service->getCoreService(),
                &name, &type, &domain,                    // Name, type, domain
                NULL, port,                             // Host and port
                f_service_text, f_service_text_length,  // TXT data, length
                NULL, 0,                                // Subtypes
                mDNSInterface_Any,                        // Interace ID
                RegistrationCallback,                   // Callback
                new_service);                           // context
    if(bstatus != mStatus_NoError)
    {
        warning("could not call mDNS_RegisterService()");
        delete new_service;
        return false;
    }
    
    log(QString("attempting to register a service called \"%1\"")
        .arg(service_name), 3);
    registered_services.append(new_service);
    return true;
}

void ZeroConfigResponder::run()
{
    mStatus status;

    status = mDNS_Init(
                        &mDNSStorage_responder, 
                        &PlatformStorage_responder,
                        mDNS_Init_NoCache, 
                        mDNS_Init_ZeroCacheSize,
                        mDNS_Init_AdvertiseLocalAddresses,
                        mDNS_Init_NoInitCallback, 
                        mDNS_Init_NoInitCallbackContext
                      );

    if (status != mStatus_NoError)
    {
        fatal("zeroconfig responder could not initialize mDNS core");
    }

    while(keep_going)
    {
        //
        //  First, check for any pending service change announcements
        //
        
        checkServiceChanges();
        
        //
        //  Second, see if we have any pending requests
        //
        
        QStringList pending_tokens;
        int         pending_socket = 0;

        //
        //  Pull off the first pending command request
        //

        things_to_do_mutex.lock();
            if(things_to_do.count() > 0)
            {
                pending_tokens = things_to_do.getFirst()->getTokens();
                pending_socket = things_to_do.getFirst()->getSocketIdentifier();
                things_to_do.removeFirst();
            }
        things_to_do_mutex.unlock();
            
        if(pending_tokens.count() > 0)
        {
            warning("has pending tokens, and should not");
        }
        else
        {        
        
            //
            //  Ask mDNS execute to do anything pending and tell in how much
            //  time it would like to be called again.
            //

            int next_time = mDNS_Execute(&mDNSStorage_responder);

            int time_difference = next_time - mDNSPlatformTimeNow();
            int seconds_to_wait = ((int) (time_difference / 1000));
            int usecs_to_wait = time_difference % 1000;
            if(seconds_to_wait < 0)
            {
                seconds_to_wait = 0;
            }
            if(usecs_to_wait < 0)
            {
                usecs_to_wait = 0;
            }
            timeout.tv_sec = seconds_to_wait;
            timeout.tv_usec = usecs_to_wait;


            int nfds = 0;
            fd_set readfds;
            FD_ZERO(&readfds);

            mDNSPosixGetFDSet(&mDNSStorage_responder, &nfds, &readfds, &timeout);

            //
            //  Add our pipe to the watcher thread as one of file descriptors it
            //  should monitor
            //

            FD_SET(u_shaped_pipe[0], &readfds);
            if(nfds <= u_shaped_pipe[0])
            {
                nfds = u_shaped_pipe[0] + 1;
            }
 
            int result = select(nfds, &readfds, NULL, NULL, &timeout);
            
            if(result < 0)
            {
                warning("got an error on select()");
            }

            if(FD_ISSET(u_shaped_pipe[0], &readfds))
            {
                u_shaped_pipe_mutex.lock();
                    char read_back[10];
                    read(u_shaped_pipe[0], read_back, 9);
                u_shaped_pipe_mutex.unlock();
            }


            mDNSPosixProcessFDSet(&mDNSStorage_responder, &readfds);
        }
    }

    log("leaving event loop", 10);

    removeAllServices();
    
    //
    //  Be nice to the rest of the network and properly remove all service
    //  from mDNS land.
    //
    
    bool keep_flushing = true;
    while(keep_flushing)
    {
        mDNS_Execute(&mDNSStorage_responder);
        
        //
        //  Give the network a bit of time to respond
        //
        
        msleep(200);
        if(registered_services.count() < 1)
        {
            keep_flushing = false;
        }
    }
}

void ZeroConfigResponder::removeAllServices()
{
    QPtrListIterator<RegisteredService> iterator(registered_services);
    RegisteredService *a_service;
    while ( (a_service = iterator.current()) != 0 )
    {
        ++iterator;
        
        mDNS_DeregisterService(&mDNSStorage_responder, a_service->getCoreService());
    }
    
}

bool ZeroConfigResponder::removeService(const QString &service_name)
{
    RegisteredService *which_one = getServiceByName(service_name);
    if(!which_one)
    {
        warning(QString("asked to remove a service it doesn't have: %1")
                .arg(service_name));
        return false;
    }
    
    mDNS_DeregisterService(&mDNSStorage_responder, which_one->getCoreService());
    return true;
}

void ZeroConfigResponder::handleServiceChange()
{

    //
    //  Something has changed w.r.t. to services. I need to query the core
    //  mfd and compare things I'm broadcasting versus it's list.
    //
    
    ServiceLister *service_lister = parent->getServiceLister();
    
    service_lister->lockBroadcastList();
    
        //
        //  Go through my services, and check that they still exist on the
        //  master list
        //
        
        QPtrListIterator<RegisteredService> iterator(registered_services);
        RegisteredService *a_service;
        while ( (a_service = iterator.current()) != 0 )
        {
            ++iterator;
            if(! service_lister->findBroadcastService(a_service->getName()) )
            {
                //
                //  Need to remove this one
                //
                
                removeService(a_service->getName());
            }
        }
        
        //
        //  Go through services on the master list, add any that I don't
        //  already know about
        //
        
        ServiceList *broadcast_list = service_lister->getBroadcastList();
        
        ServiceList::iterator it;
        for ( it  = broadcast_list->begin(); 
              it != broadcast_list->end(); 
              ++it )
        {
            RegisteredService *checker = getServiceByName((*it).getName());
            if(!checker)
            {
                //
                //  This one is new, I should begin broadcasting it
                //
                
                registerService((*it).getName(), (*it).getType(), (*it).getPort());
                
            }
        }

    service_lister->unlockBroadcastList();
}

ZeroConfigResponder::~ZeroConfigResponder()
{
    registered_services.clear();
}

