#ifndef DAAPSERVER_H_
#define DAAPSERVER_H_
/*
	daapserver.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the daap server
*/

#include <iostream>
using namespace std; 

#include <qvaluelist.h>

#include "mfd_plugin.h"
#include "../../mdserver.h"

struct httpd;

#include "daaplib/basic.h"
#include "daaplib/tagoutput.h"

#include "request.h"
#include "session.h"

class DaapServer: public MFDHttpPlugin
{

  public:

    DaapServer(MFD *owner, int identity);
    ~DaapServer();

    void    handleIncoming(HttpInRequest *request, int client_id);
    void    parsePath(HttpInRequest *http_request, DaapRequest *daap_request);
    void    sendServerInfo(HttpInRequest *http_request, DaapRequest *daap_request);
    void    sendContentCodes(HttpInRequest *http_request);
    void    sendTag(HttpInRequest *http_request, const Chunk& c);
    void    sendLogin(HttpInRequest *http_request, u32 session_id);
    void    parseVariables(HttpInRequest *http_request, DaapRequest *daap_request);
    void    sendUpdate(HttpInRequest *http_request, u32 database_version);
    void    sendMetadata(HttpInRequest *http_request, QString request_path, DaapRequest *daap_request);
    void    sendDatabaseList(HttpInRequest *http_request);
    void    addItemToResponse(HttpInRequest *http_request, DaapRequest *daap_request, TagOutput &response, AudioMetadata *which_item, u64 meta_codes);
    void    sendDatabase(HttpInRequest *http_request, DaapRequest *daap_request, int which_database);
    void    sendDatabaseItem(HttpInRequest *http_request, u32 song_id, DaapRequest *daap_request);
    void    sendContainers(HttpInRequest *http_request, DaapRequest *daap_request, int which_database);
    void    sendContainer(HttpInRequest *http_request, u32 container_id, int which_database);
    void    addPlaylistWithinPlaylist(TagOutput &response, HttpInRequest *http_request, int which_playlist, int which_container);
    void    handleMetadataChange(int which_collection);

    DaapSessions daap_sessions;

  private:
  
    MetadataServer              *metadata_server;
    QPtrList<MetadataContainer> *metadata_containers;
    QString                     service_name;
    QValueList<int>             hanging_updates;
    QMutex                      hanging_updates_mutex;
};

#endif  // daapserver_h_
