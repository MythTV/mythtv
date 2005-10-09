#ifndef LCDSERVER_H_
#define LCDSERVER_H_
/*
	lcdserver.h

	Headers for the core lcdserver object

*/

#include <qobject.h>
#include <qstringlist.h>

#include "serversocket.h"
#include "lcdprocclient.h"

/*
  control how much debug info we get
  0 = none
  1 = LCDServer info & LCDd info 
  2 = screen switch info
  10 = every command sent and error received
*/

extern int debug_level;

class LCDServer : public QObject
{

    Q_OBJECT

  public:
  
    LCDServer(int port, QString message, int messageTime);

    void sendKeyPress(QString key_pressed);
     
  signals:
  
  private slots:
  
    void newConnection(QSocket *);
    void endConnection(QSocket *);
    void readSocket();
    QStringList parseCommand(QString &command);
    void parseTokens(const QStringList &tokens, QSocket *socket);
    void shutDown();
    void sendMessage(QSocket *where, const QString &what);
    void sendConnected(QSocket *socket);
    void switchToTime(QSocket *socket);
    void switchToMusic(const QStringList &tokens, QSocket *socket);
    void switchToGeneric(const QStringList &tokens, QSocket *socket);
    void switchToChannel(const QStringList &tokens, QSocket *socket);
    void switchToVolume(const QStringList &tokens, QSocket *socket);
    void switchToNothing(QSocket *socket);
    void switchToMenu(const QStringList &tokens, QSocket *socket);
    void setChannelProgress(const QStringList &tokens, QSocket *socket);
    void setMusicProgress(const QStringList &tokens, QSocket *socket);
    void setGenericProgress(const QStringList &tokens, QSocket *socket);
    void setVolumeLevel(const QStringList &tokens, QSocket *socket);
    void updateLEDs(const QStringList &tokens, QSocket *socket);
    
  private:

    LCDProcClient   *m_lcd;
    LCDServerSocket *m_serverSocket;
    QSocket         *m_lastSocket;        // last socket we received data from
};

#endif

