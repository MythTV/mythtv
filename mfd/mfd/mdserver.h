#ifndef MDSERVER_H_
#define MDSERVER_H_
/*
	mdserver.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a threaded object that opens a server
	port and explains the state of the metadata to any
	client that asks

*/


#include "../mfdlib/mfd_plugin.h"

class MFD;

class MetadataServer : public MFDServicePlugin
{
    //
    //  Don't be confused by the fact that this inherits from
    //  MFDServcePlugin. It is not dynamically loaded, but is part of the
    //  mfd core (just so happens that MFDServicePlugin has most of the bits
    //  and pieces we need for a metadata serving object already built in).
    //

  public:
  
    MetadataServer(MFD *owner, int port);
    ~MetadataServer();
    void    run();
    void    doSomething(const QStringList &tokens, int socket_identifier);
    void    startMetadataCheck();

  private:
  
    bool    checkMetadata();
    
};


#endif
