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

class DaapClient;
class DaapResponse;

class DaapInstance: public QThread
{

  public:

    DaapInstance(
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

  private:

  
    bool        keep_going;
    QMutex      keep_going_mutex;
    DaapClient  *parent;
    QString     server_address;
    uint        server_port;
    QString     service_name;
    QMutex      u_shaped_pipe_mutex;
    int         u_shaped_pipe[2];

    QSocketDevice *client_socket_to_daap_server;

};

#endif  // daapinstance_h_
