#ifndef LCDSERVER_H_
#define LCDSERVER_H_
/*
    lcdserver.h

    Headers for the core lcdserver object

*/

#include <QStringList>
#include <QObject>
#include <QTcpSocket>

#include "libmythbase/serverpool.h"
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

    LCDServer(int port, QString message, std::chrono::seconds messageTime);

    void sendKeyPress(const QString& key_pressed);

  signals:

  private slots:

    void newConnection(QTcpSocket *socket);
    void endConnection(void);
    void readSocket();
    static QStringList parseCommand(QString &command);
    void parseTokens(const QStringList &tokens, QTcpSocket *socket);
    void shutDown();
    static void sendMessage(QTcpSocket *where, const QString &what);
    void sendConnected(QTcpSocket *socket);
    void switchToTime(QTcpSocket *socket);
    void switchToMusic(const QStringList &tokens, QTcpSocket *socket);
    void switchToGeneric(const QStringList &tokens, QTcpSocket *socket);
    void switchToChannel(const QStringList &tokens, QTcpSocket *socket);
    void switchToVolume(const QStringList &tokens, QTcpSocket *socket);
    void switchToNothing(QTcpSocket *socket);
    void switchToMenu(const QStringList &tokens, QTcpSocket *socket);
    void setChannelProgress(const QStringList &tokens, QTcpSocket *socket);
    void setMusicProgress(const QStringList &tokens, QTcpSocket *socket);
    void setMusicProp(const QStringList &tokens, QTcpSocket *socket);
    void setGenericProgress(const QStringList &tokens, QTcpSocket *socket);
    void setVolumeLevel(const QStringList &tokens, QTcpSocket *socket);
    void updateLEDs(const QStringList &tokens, QTcpSocket *socket);

  private:

    LCDProcClient   *m_lcd        { nullptr };
    ServerPool      *m_serverPool { nullptr };
    QTcpSocket      *m_lastSocket { nullptr };  // last socket we received data from

};

#endif

