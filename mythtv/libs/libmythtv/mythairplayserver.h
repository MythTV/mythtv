#ifndef MYTHAIRPLAYSERVER_H
#define MYTHAIRPLAYSERVER_H

#include <QObject>
#include <QUrl>

#include "serverpool.h"
#include "mythtvexp.h"

class QMutex;
class MThread;
class BonjourRegister;

#define AIRPLAY_PORT_RANGE 100

enum AirplayEvent
{
    AP_EVENT_NONE = -1,
    AP_EVENT_PLAYING = 0,
    AP_EVENT_PAUSED  = 1,
    AP_EVENT_LOADING = 2,
    AP_EVENT_STOPPED = 3,
};

class AirplayConnection
{
  public:
    AirplayConnection()
      : controlSocket(NULL), reverseSocket(NULL), speed(1.0f),
        position(0.0f), url(QUrl()), lastEvent(AP_EVENT_NONE),
        stopped(false)
    { }
    QTcpSocket  *controlSocket;
    QTcpSocket  *reverseSocket;
    float        speed;
    double       position;
    QUrl         url;
    AirplayEvent lastEvent;
    bool         stopped;
};

class APHTTPRequest;

class MTV_PUBLIC MythAirplayServer : public ServerPool
{
    Q_OBJECT

  public:
    static bool Create(void);
    static void Cleanup(void);

    MythAirplayServer();

  private slots:
    void Start();
    void newConnection(QTcpSocket *client);
    void deleteConnection();
    void read();

  private:
    virtual ~MythAirplayServer(void);
    void Teardown(void);
    void HandleResponse(APHTTPRequest *req, QTcpSocket *socket);
    QByteArray StatusToString(int status);
    QString eventToString(AirplayEvent event);
    void GetPlayerStatus(bool &playing, float &speed, double &position,
                         double &duration);
    QString GetMacAddress(QTcpSocket *socket);
    void SendReverseEvent(QByteArray &session, AirplayEvent event);

    // Globals
    static MythAirplayServer *gMythAirplayServer;
    static QMutex            *gMythAirplayServerMutex;
    static MThread           *gMythAirplayServerThread;

    // Members
    QString          m_name;
    BonjourRegister *m_bonjour;
    bool             m_valid;
    QMutex          *m_lock;
    int              m_setupPort;
    QList<QTcpSocket*> m_sockets;
    QHash<QByteArray,AirplayConnection> m_connections;
};

#endif // MYTHAIRPLAYSERVER_H
