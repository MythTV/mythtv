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
  
    void newConnection(Q3Socket *);
    void endConnection(Q3Socket *);
    void readSocket();
    QStringList parseCommand(QString &command);
    void parseTokens(const QStringList &tokens, Q3Socket *socket);
    void shutDown();
    void sendMessage(Q3Socket *where, const QString &what);
    void sendConnected(Q3Socket *socket);
    void switchToTime(Q3Socket *socket);
    void switchToMusic(const QStringList &tokens, Q3Socket *socket);
    void switchToGeneric(const QStringList &tokens, Q3Socket *socket);
    void switchToChannel(const QStringList &tokens, Q3Socket *socket);
    void switchToVolume(const QStringList &tokens, Q3Socket *socket);
    void switchToNothing(Q3Socket *socket);
    void switchToMenu(const QStringList &tokens, Q3Socket *socket);
    void setChannelProgress(const QStringList &tokens, Q3Socket *socket);
    void setMusicProgress(const QStringList &tokens, Q3Socket *socket);
    void setMusicProp(const QStringList &tokens, Q3Socket *socket);
    void setGenericProgress(const QStringList &tokens, Q3Socket *socket);
    void setVolumeLevel(const QStringList &tokens, Q3Socket *socket);
    void updateLEDs(const QStringList &tokens, Q3Socket *socket);
    
  private:

    LCDProcClient   *m_lcd;
    LCDServerSocket *m_serverSocket;
    Q3Socket         *m_lastSocket;        // last socket we received data from
};

#endif

