#ifndef REQUEST_THREAD_H_
#define REQUEST_THREAD_H_
/*
	requestthread.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Little threads that make some of the plugin classes multi-threaded

*/

#include "clientsocket.h"

class MFDServicePlugin;

class ServiceRequestThread : public QThread
{

 public:

    ServiceRequestThread(MFDServicePlugin *owner);

    virtual void    run();
    void            handleIncoming(MFDServiceClientSocket *socket);
    void            killMe();

  private:

    MFDServicePlugin        *parent;

    MFDServiceClientSocket  *client_socket;
    bool                    do_stuff;
    QMutex                  do_stuff_mutex;

    QWaitCondition          wait_condition;
    bool                    keep_going;
    QMutex                  keep_going_mutex;
};



#endif

