#ifndef MFDINSTANCE_H_
#define MFDINSTANCE_H_
/*
	mfdinstance.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	You get one of these objects for every mfd in existence

*/

#include <qthread.h>
#include <qmutex.h>
#include <qsocketdevice.h>
#include <qstringlist.h>

#include "serviceclient.h"

class MfdInterface;

class MfdInstance : public QThread
{

  public:

    MfdInstance(
                int an_id,
                MfdInterface *the_interface,
                const QString &l_name,
                const QString &l_hostname, 
                const QString &l_ip_address,
                int l_port
               );
    ~MfdInstance();
    
    void run();
    void stop();
    void wakeUp();
    
    QString getName(){return name;}
    QString getHostname(){return hostname;}
    QString getAddress(){return ip_address;}
    int     getPort(){return port;}
    void    readFromMfd();
    void    parseFromMfd(QStringList &tokens);
    void    announceMyDemise();
    int     getId(){return mfd_id;}
    void    commitListEdits(
                            UIListGenericTree *playlist_tree,
                            bool new_list,
                            QString playlist_name
                           );
    
    //
    //  Adding and removing interfaces to services _this_ mfd offers
    //
    
    void addAudioClient(const QString &address, uint a_port);
    void addMetadataClient(const QString &address, uint a_port);
    void addRtspClient(const QString &address, uint a_port);
    void addMtcpClient(const QString &address, uint a_port);

    void removeServiceClient(
                                MfdServiceType type, 
                                const QString &address, 
                                uint a_port
                            );

    virtual void    addPendingCommand(QStringList new_command);

    //
    //  Do we have a transcoder 
    //
    
    bool hasTranscoder();
 
  private:
  
    void    executeCommands(QStringList new_command);    

    QString name;
    QString hostname;
    QString ip_address;
    int     port;
    
    bool    keep_going;
    QMutex  keep_going_mutex;

    QMutex  u_shaped_pipe_mutex;
    int     u_shaped_pipe[2];

    QSocketDevice *client_socket_to_mfd;

    QPtrList<ServiceClient> *my_service_clients;
    
    MfdInterface *mfd_interface;
    int mfd_id;

    //
    //  Thread safe way to get commands from the client thread into this
    //  instance thread for execution
    //    

    QValueList<QStringList> pending_commands;
    QMutex                  pending_commands_mutex;

};

#endif
