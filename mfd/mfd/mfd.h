#ifndef MFD_H_
#define MFD_H_
/*
	mfd.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the core Myth Frontend Daemon

*/

#include <iostream>
using namespace std;

#include <qobject.h>
#include <qsqldatabase.h>
#include <qstringlist.h>

#include "pluginmanager.h"
#include "serversocket.h"
#include "logging.h"

class MFD : public QObject
{

  Q_OBJECT

    //
    //  Load plugin modules, wait for connections, 
    //  launch threads, expose services, etc.
    //

  public:
  
    MFD(QSqlDatabase *db, int port, bool log_stdout, int logging_verbosity);
    ~MFD();
    
    void shallowPrintHello(){cout << "This is a shallow hello from the mfd" << endl;}
    void deepPrintHello();
    
    
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

    void log(const QString &log_text, int verbosity);
    void warning(const QString &warning_text);
    void error(const QString &error_text);
    void parseTokens(const QStringList &tokens, MFDClientSocket *socket);
    void shutDown();
    void sendMessage(MFDClientSocket *where, const QString &what);
    void sendMessage(const QString &the_message);
    void doListCapabilities(const QStringList &tokens, MFDClientSocket *socket);

    QSqlDatabase               *db;
    MFDLogger                  *mfd_log;
    MFDPluginManager           *plugin_manager;
    MFDServerSocket            *server_socket;
    QPtrList <MFDClientSocket> connected_clients;
    bool                       shutting_down;    
    bool                       watchdog_flag;
    int                        port_number;
};

#endif  // mfd_h_

