#ifndef DAAPINSTANCE_H_
#define DAAPINSTANCE_H_
/*
	daapinstance.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object that knows how to talk to daap servers

*/

#include <qthread.h>
#include <qsocketdevice.h>
#include <qptrlist.h>

#include "../daapserver/daaplib/taginput.h"
#include "../daapserver/daaplib/tagoutput.h"


#include "database.h"

class DaapClient;
class DaapResponse;

#include "server_types.h"

class DaapInstance: public QThread
{

  public:

    DaapInstance(
                    MFD *my_mfd,
                    DaapClient *owner, 
                    const QString &l_server_address, 
                    uint l_server_port,
                    const QString &l_service_name
                );

    ~DaapInstance();

    void    run();
    void    stop();
    bool    keepGoing();
    void    wakeUp();
    void    log(const QString &log_message, int verbosity);
    void    warning(const QString &warn_message);
    void    handleIncoming();
    void    processResponse(DaapResponse *daap_response);
    bool    checkServerType(const QString &server_description);
    int     getDaapServerType(){return daap_server_type; }
    bool    isThisYou(QString a_name, QString an_address, uint a_port);
    int     getDaapVersionMajor(){return daap_version.hi;}
    int     getSessionId(){return session_id;}
    QString getName(){ return service_name;}

    //
    //  Methods to handle the various responses we can get from a DAAP
    //  server
    //
    
    void doServerInfoResponse(TagInput& dmap_data);
    void doLoginResponse(TagInput &dmap_data);
    void doUpdateResponse(TagInput &dmap_data);
    void doDatabaseListResponse(TagInput &dmap_data);
    void parseDatabaseListings(TagInput &dmap_data, int how_many);
    
  private:

  
    bool        keep_going;
    QMutex      keep_going_mutex;
    MFD         *the_mfd;
    DaapClient  *parent;
    QMutex      service_details_mutex;
    QString     server_address;
    uint        server_port;
    QString     service_name;
    QMutex      u_shaped_pipe_mutex;
    int         u_shaped_pipe[2];

    DaapServerType daap_server_type;
    QSocketDevice  *client_socket_to_daap_server;

    QPtrList<Database>  databases;
    Database            *current_request_db;
    int                 current_request_playlist;

    //
    //  A WHOLE bunch of values that are filled in by talking to the server
    //


    //
    //  from /server-info
    //

    bool    have_server_info;

    QString supplied_service_name;
    bool    server_requires_login;
    int     server_timeout_interval;
    bool    server_supports_autologout;
    int     server_authentication_method;
    bool    server_supports_update;
    bool    server_supports_persistentid;
    bool    server_supports_extensions;
    bool    server_supports_browse;
    bool    server_supports_query;
    bool    server_supports_index;
    bool    server_supports_resolve;
    int     server_numb_databases;
    Version daap_version;
    Version dmap_version;
    
    //
    //  from /login
    //

    bool    logged_in;
    int     session_id;
    
    //
    //  from /update
    //
    
    int metadata_generation;
};

#endif  // daapinstance_h_
