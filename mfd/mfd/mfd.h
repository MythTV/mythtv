#ifndef MFD_H_
#define MFD_H_
/*
	mfd.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the core Myth Frontend Daemon

*/

#include <qobject.h>
#include <qstringlist.h>

#include "pluginmanager.h"
#include "serversocket.h"
#include "logging.h"
#include "mdcontainer.h"

class MetadataServer;
class ServiceLister;

class MFD : public QObject
{

  Q_OBJECT

    //
    //  Load plugin modules, wait for connections, 
    //  launch threads, expose services, etc.
    //

  public:
  
    MFD(uint port, bool log_stdout, int logging_verbosity);
    ~MFD();
    
    MetadataServer*     getMetadataServer(){return metadata_server;}
    ServiceLister*      getServiceLister(){ return service_lister; }
    MFDPluginManager*   getPluginManager(){return plugin_manager;}
    void                log(const QString &log_text, int verbosity);
    void                warning(const QString &warning_text);
    void                sendMessage(const QString &the_message);
    
  public slots:
  
    void registerMFDService();

  signals:
  
    void writeToLog(int, const QString &log_text);

  protected:
  
    void customEvent(QCustomEvent *ce);

  private slots:
  
    void newConnection(MFDClientSocket *);
    void endConnection(MFDClientSocket *);
    void readSocket();
    void raiseWatchdogFlag();

  private:

    void error(const QString &error_text);
    void parseTokens(const QStringList &tokens, MFDClientSocket *socket);
    void shutDown();
    void sendMessage(MFDClientSocket *where, const QString &what);
    void doListCapabilities(const QStringList &tokens, MFDClientSocket *socket);
    void doServiceResponse(const QStringList &tokens, MFDClientSocket *socket);
    void doServiceList(MFDClientSocket *socket);

    MetadataServer              *metadata_server;
    ServiceLister               *service_lister;
    MFDLogger                   *mfd_log;
    MFDPluginManager            *plugin_manager;
    MFDServerSocket             *server_socket;
    QPtrList <MFDClientSocket>  connected_clients;
    bool                        shutting_down;    
    bool                        watchdog_flag;
    uint                        port_number;
};

#endif  // mfd_h_

