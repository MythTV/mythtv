#ifndef MFD_PLUGIN_H_
#define MFD_PLUGIN_H_
/*
	mfd_plugin.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the plugin(s) skeleton(s).

*/

#include <qapplication.h>
#include <qobject.h>
#include <qthread.h>
#include <qmutex.h>
#include <qstringlist.h>
#include <qsocketdevice.h>
#include <vector>
using namespace std;

#include "../mfd/mfd.h"
#include "requestthread.h"
#include "clientsocket.h"
#include "httprequest.h"
 
class SocketBuffer
{
    //
    //  An mfd plugin may be busy when it gets
    //  passed tokens to parse. This is a little
    //  class to let it store these requests until
    //  it has time to deal with them.
    //

  public: 
  
    SocketBuffer(const QStringList &l_tokens, int l_socket_identifier)
    {
        tokens = l_tokens;
        socket_identifier = l_socket_identifier; 
    }

    const QStringList& getTokens(){return tokens;}
    int                getSocketIdentifier(){return socket_identifier;}
    
  private:
  
    QStringList tokens;
    int         socket_identifier;
    
};

class MFDBasePlugin : public QThread
{
    
    //
    //  This is a base class for MFD Plugin objects. You don't *have* to
    //  inherit any of these classes to write a plugin (you can use any code
    //  that adheres to the plugin interface), it just makes it easier.
    //

  public:

    MFDBasePlugin(MFD* owner, int identifier);
    ~MFDBasePlugin();

    virtual void stop(); 
    virtual void wakeUp();
    void         log(const QString &log_message, int verbosity);
    void         warning(const QString &warning_message);
    void         fatal(const QString &death_rattle);
    virtual void sendMessage(const QString &message, int socket_identifier);
    virtual void sendMessage(const QString &message);
    virtual void huh(const QStringList &tokens, int socket_identifier);
    void         setName(const QString &a_name);
    

  protected:
  
    MFD                     *parent;
    int                     unique_identifier;

    QMutex                  log_mutex;
    QMutex                  warning_mutex;
    QMutex                  fatal_mutex;
    QMutex                  socket_mutex;
    QMutex                  allclient_mutex;

    bool                    keep_going;
    QMutex                  keep_going_mutex;

    QWaitCondition          main_wait_condition;
    QMutex                  main_wait_mutex;
    
    QString                 name;
    
};

class MFDCapabilityPlugin : public MFDBasePlugin
{
    
  public:


    MFDCapabilityPlugin(MFD* owner, int identifier);
    ~MFDCapabilityPlugin();

    virtual void doSomething(const QStringList &tokens, int socket_identifier);
    void         parseTokens(const QStringList &tokens, int socket_identifier);
    QStringList  getCapabilities();
    virtual void run();
    virtual void runOnce(){;}   //  called after constructor at start of run()    

  protected:
  
    QStringList             my_capabilities;

    QPtrList<SocketBuffer>  things_to_do;
    QMutex                  things_to_do_mutex;


};

class MFDServicePlugin : public MFDBasePlugin
{
  public:
  
    MFDServicePlugin(MFD *owner, int identifier, int port, bool l_use_thread_pool = true, uint l_thread_pool_size = 5);
    ~MFDServicePlugin();
    
    virtual void    run();
    bool            initServerSocket();
    void            updateSockets();
    void            findNewClients();
    void            dropDeadClients();
    void            readClients();
    virtual void    doSomething(const QStringList &tokens, int socket_identifier);
    void            wakeUp();
    void            waitForSomethingToHappen();
    void            setTimeout(int numb_seconds, int numb_useconds);
    int             bumpClient();
    void            sendCoreMFDMessage(const QString &message, int socket_identifier);
    void            sendCoreMFDMessage(const QString &message);
    void            sendMessage(const QString &message, int socket_identifier);
    void            sendMessage(const QString &message);


    virtual void    processRequest(MFDServiceClientSocket *a_client);
    void            markUnused(ServiceRequestThread *which_one);

  protected:

    int                     port_number;
    
    //
    //  One server socket, and a collection of client socket, plus a mutex
    //  to protect changes to the list of client sockets.
    //

    QSocketDevice                    *core_server_socket;
    QPtrList<MFDServiceClientSocket> client_sockets;
    int                              client_socket_identifier;
    QMutex                           client_sockets_mutex;


    //
    //  A time structure for how long we sit in select() calls when nothing
    //  else is going on. Note that this can be set to a really long time,
    //  since one of the descriptors that select() pays attention to is a
    //  pipe (see below).
    //

    struct  timeval timeout;
    int     time_wait_seconds;
    int     time_wait_usecs;
    QMutex  timeout_mutex;

    //
    //  Insanely clever. A pipe from this object to this object. When
    //  nothing else is going on, the execution loop sits in select(). But
    //  one of the descriptors it pays attention to is the read side of this
    //  u_shaped pipe. Any other thread can wake us up just by sitting a few
    //  arbitrary bytes into the write end (ie. calling wakeUp()).
    //

    QMutex  u_shaped_pipe_mutex;
    int     u_shaped_pipe[2];

    //
    //  A pool of threads to process incoming requests.
    //

    vector<ServiceRequestThread *> thread_pool;
    QMutex                         thread_pool_mutex;
    uint                           thread_pool_size;
    bool                           use_thread_pool;
};

class MFDHttpPlugin : public MFDServicePlugin
{

  public:
  
    MFDHttpPlugin(MFD *owner, int identifier, int port);
    ~MFDHttpPlugin();

    void            run();
    virtual void    processRequest(MFDServiceClientSocket *a_client);
    virtual void    handleIncoming(HttpRequest *request);
    virtual void    sendResponse(int client_id, HttpResponse *http_response);
};

#endif

