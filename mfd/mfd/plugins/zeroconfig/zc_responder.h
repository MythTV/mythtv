#ifndef ZC_RESPONDER_H_
#define ZC_RESPONDER_H_
/*
	zc_responder.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project

    !! This code is covered the the Apple Public Source License !!
    
    See ./apple/APPLE_LICENSE
	
*/

#include <qthread.h>
#include <qptrlist.h>

#include "./apple/mDNSClientAPI.h"// Defines the interface to the mDNS core code
#include "./apple/mDNSPosix.h"    // Defines the specific types needed to run mDNS on this platform

#include "../../mfd.h"


//
//  A little class to hold information about services that are registered
//  and/or trying to get register
//

class RegisteredService
{
  public:
  
    RegisteredService(
                        int id, 
                        QString name
                     );
    
    ServiceRecordSet*   getCoreService(){return &core_service;}
    const QString &     getName(){return service_name;}    
    void                setRegistered(bool yes_or_no){registered = yes_or_no;}

  private:
  
    int                 unique_identifier;
    ServiceRecordSet    core_service;
    bool                registered;
    QString             service_name;
};

//
//  The Responder is responsible for advertising the services available on
//  this mfd.
//

class ZeroConfigResponder: public MFDBasePlugin
{

  public:

    ZeroConfigResponder(MFD *owner, int identifier);
    ~ZeroConfigResponder();
    void run();
    bool formatServiceText(
                            const char *serviceText,
                            mDNSu8 *pStringList,
                            mDNSu16 *pStringListLen
                          );

    bool registerService(
                            const QString& service_name,
                            const QString& service_type,
                            uint  port_number,
                            const QString& service_domain,
                            const QString& service_text
                        );
    bool removeService(const QString &service_name);

    void handleRegistrationCallback(
                                    mDNS *const m, 
                                    ServiceRecordSet *const thisRegistration, 
                                    mStatus status
                                   );

    RegisteredService*  getServiceByName(QString name);
    void addRequest(const QStringList &tokens, int socket_identifier);
    void doSomething(const QStringList &tokens, int socket_identifier);
    
  private:
  
    int         bumpIdentifiers(){++service_identifier; return service_identifier;}

    MFD     *parent;
    int     unique_identifier;

    int     service_identifier;

    QPtrList<RegisteredService>   registered_services;

    QPtrList<SocketBuffer>  things_to_do;
    QMutex                  things_to_do_mutex;

    MFDFileDescriptorWatchingPlugin *fd_watcher;
    QMutex file_descriptors_mutex;

};

#endif  // zc_responder_h_


