#ifndef MFD_PLUGIN_H_
#define MFD_PLUGIN_H_
/*
	mfd_plugin.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the plugin skeleton.

*/

#include <qapplication.h>
#include <qobject.h>
#include <qthread.h>
#include <qmutex.h>
#include <qstringlist.h>
#include <qsocketdevice.h>

#include "../mfd/mfd.h"
 
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

class MFDServiceClientSocket : public QSocketDevice
{
  public:
  
    MFDServiceClientSocket(int identifier, int socket_id, Type type);
    int getIdentifier(){return unique_identifier;}
    
  private:
  
    int unique_identifier;
};

class MFDServicePlugin : public MFDBasePlugin
{
  public:
  
    MFDServicePlugin(MFD *owner, int identifier, int port);
    ~MFDServicePlugin();
    
    bool    initServerSocket();
    void    updateSockets();
    void    findNewClients();
    void    dropDeadClients();
    void    readClients();
    void    addToDo(QString new_stuff_todo, int client_id);
    void    wakeUp();
    void    addFileDescriptor(int fd);
    void    removeFileDescriptor(int fd);
    void    clearFileDescriptors();
    void    waitForSomethingToHappen();
    void    setTimeout(int numb_seconds, int numb_useconds);

    int     bumpClient(){
                            ++client_socket_identifier;
                            return client_socket_identifier;
                        }
    void    sendMessage(const QString &message, int socket_identifier);
    void    sendMessage(const QString &message);

  protected:

    int                     port_number;
    QPtrList<SocketBuffer>  things_to_do;
    QMutex                  things_to_do_mutex;
    
    QSocketDevice                    *core_server_socket;
    QPtrList<MFDServiceClientSocket> client_sockets;
    int                              client_socket_identifier;

    struct  timeval timeout;
    int     time_wait_seconds;
    int     time_wait_usecs;
    QMutex  timeout_mutex;

    QMutex  u_shaped_pipe_mutex;
    int     u_shaped_pipe[2];

    QMutex          file_descriptors_mutex;
    QValueList<int> internal_file_descriptors;
    QValueList<int> extra_file_descriptors;


};

#endif

