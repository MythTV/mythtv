#ifndef DAAPSERVER_H_
#define DAAPSERVER_H_
/*
	daapserver.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the daap server
*/

#include "mfd_plugin.h"

struct httpd;

#include "daaplib/basic.h"

#include "request.h"
#include "session.h"

class DaapServer: public MFDServicePlugin
{

  public:

    DaapServer(MFD *owner, int identity);
    ~DaapServer();
    
    void    run();
    void    parseIncomingRequest(httpd *server);
    void    parsePath(httpd *server, DaapRequest *daap_request);
    void    sendServerInfo(httpd *server);
    void    sendTag(httpd *server, const Chunk& c);
    void    sendLogin(httpd *server, u32 session_id);
    void    parseVariables(httpd *server, DaapRequest *daap_request);
    void    sendUpdate(httpd *server, u32 database_version);
    void    sendMetadata(httpd *server, QString request_path, DaapRequest *daap_request);
    void    sendDatabaseList(httpd *server);
    void    sendDatabase(httpd *server, DaapRequest *daap_request, int which_database);
    void    sendDatabaseItem(httpd *server, u32 song_id);
    void    sendContainers(httpd *server, DaapRequest *daap_request, int which_database);
    void    sendContainer(httpd *server, u32 container_id, int which_database);
    bool    wantsToContinue(){return keep_going;}

    DaapSessions daap_sessions;

  private:
  
    QPtrList<MetadataContainer> *metadata_containers;

    QString service_name;

};

#endif  // daapserver_h_
