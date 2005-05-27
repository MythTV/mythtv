#ifndef MFD_PLUGIN_H_
#define MFD_PLUGIN_H_
/*
	mfd_plugin.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the plugin(s) skeleton(s).

*/

#include <qapplication.h>
#include <qobject.h>
#include <qthread.h>
#include <qmutex.h>
#include <qstringlist.h>
#include <qptrlist.h>
#include <qvaluelist.h>
#include <qsocketdevice.h>
#include <deque>
#include <vector>
using namespace std;

#include "../mfd/mfd.h"
#include "requestthread.h"
#include "clientsocket.h"
#include "httpinrequest.h"
#include "mfd_events.h"
 
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

class MetadataEvent
{
    //
    //  Similarly, mfdplugins can get passed metadata (changed) events by
    //  other processes outside of their main thread(s). This is a tiny
    //  class for storing those events until the main thread(s) can deal
    //  with them (each mfd_plugin has a class member which is a
    //  std::deque<MetadataEvent> for pushing events onto the end of the
    //  queue and pulling them off the front when there is time to process
    //  them). The "external" flag just indicates if the metadata collection
    //  was changed by an object that does not "own" it.
    
  public:
    
    MetadataEvent(int l_collection_id, bool l_external)
    {
        collection_id = l_collection_id;
        external = l_external;
    }
    ~MetadataEvent(){;}
    
    int     getCollectionId(){ return collection_id; }
    bool    getExternal(){ return external; }
    
  private:
  
    int     collection_id;
    bool    external;
    
};

class MFDBasePlugin : public QThread
{
    
    //
    //  This is a base class for MFD Plugin objects. You don't *have* to
    //  inherit any of these classes to write a plugin (you can use any code
    //  that adheres to the plugin interface), it just makes it easier.
    //

  public:

    MFDBasePlugin(MFD* owner, int identifier, const QString &a_name = "unkown");
    virtual ~MFDBasePlugin();

    virtual void stop(); 
    virtual void wakeUp();
    void         log(const QString &log_message, int verbosity);
    void         warning(const QString &warning_message);
    void         fatal(const QString &death_rattle);
    virtual void sendMessage(const QString &message, int socket_identifier);
    virtual void sendMessage(const QString &message);
    virtual void huh(const QStringList &tokens, int socket_identifier);
    bool         keepGoing();
    void         metadataChanged(int which_collection, bool external=false);
    void         servicesChanged();
    MFD*         getMfd(){return parent;}
    

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
    QString                 hostname;
    
    bool                    services_changed_flag;
    QMutex                  services_changed_mutex;

    QMutex                  metadata_changed_mutex;
    int                     metadata_collection_last_changed;
    std::deque<MetadataEvent>   metadata_events;
};

class MFDCapabilityPlugin : public MFDBasePlugin
{
    
  public:


    MFDCapabilityPlugin(MFD* owner, int identifier, const QString &a_name = "unkown");
    virtual ~MFDCapabilityPlugin();

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
  
    MFDServicePlugin(
                        MFD *owner, 
                        int identifier, 
                        uint port, 
                        const QString &a_name = "unkown",
                        bool l_use_thread_pool = true, 
                        uint l_minimum_thread_pool_size = 0
                    );
    virtual ~MFDServicePlugin();
    
    virtual void    run();
    virtual void    stop();
    bool            initServerSocket();
    void            updateSockets();
    void            findNewClients();
    void            dropDeadClients();
    void            readClients();
    virtual void    doSomething(const QStringList &tokens, int socket_identifier);
    void            wakeUp();
    void            checkThreadPool();
    void            waitForSomethingToHappen();
    void            setTimeout(int numb_seconds, int numb_useconds);
    int             bumpClient();
    void            sendMessage(const QString &message, int socket_identifier);
    void            sendMessage(const QString &message);
    void            sendInternalMessage(const QString &the_message);
    void            checkInternalMessages();
    void            checkMetadataChanges();
    void            checkServiceChanges();
    virtual void    handleInternalMessage(QString the_message);

    virtual void    processRequest(MFDServiceClientSocket *a_client);
    void            markUnused(ServiceRequestThread *which_one);
    virtual void    handleMetadataChange(int which_collection, bool external=false);
    virtual void    handleServiceChange();
    virtual void    addFileDescriptorToWatch(int fd);
    virtual void    removeFileDescriptorToWatch(int fd);
    
  protected:

    uint    port_number;
    
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
    //  u_shaped pipe. Any other thread can wake us up just by sending a few
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
    uint                           minimum_thread_pool_size;
    bool                           use_thread_pool;
    
    //
    //  A time check to see how long it's been since we last used all our
    //  threads. If "enough" time has gone by, we can lower the number.
    //
    
    QTime thread_exhaustion_timestamp;

    //
    //  A little queue of "internal" messages. These are things that
    //  sub-threads want to be able to pass to each other. An example would
    //  be where an audio decoder wants to tell the main audio plugin thread
    //  that it has finished playing something.
    //


    QValueList<QString>  internal_messages;
    QMutex             internal_messages_mutex;

    //
    //  A mutex and a value list of other file descriptors to watch when the
    //  pluging goes into its select() call during
    //  waitForSomethingToHappen()
    //

    QMutex                  other_file_descriptors_mutex;
    QValueList<int>         other_file_descriptors_to_watch;    

};

#endif

