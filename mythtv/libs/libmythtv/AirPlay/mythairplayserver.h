#ifndef MYTHAIRPLAYSERVER_H
#define MYTHAIRPLAYSERVER_H

#include <QObject>
#include <QUrl>
#include <stdint.h>     // for uintxx_t

#include "serverpool.h"
#include "mythtvexp.h"

class QMutex;
class MThread;
class BonjourRegister;

#define AIRPLAY_PORT_RANGE 100
#define AIRPLAY_HARDWARE_ID_SIZE 6
QString     AirPlayHardwareId(void);
QString     GenerateNonce(void);
QByteArray  DigestMd5Response(QString response, QString option,
                              QString nonce, QString password,
                              QByteArray &auth);

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
        position(0.0f), initial_position(-1.0f), url(QUrl()),
        lastEvent(AP_EVENT_NONE), stopped(false), was_playing(false),
        initialized(false)
    { }
    QTcpSocket  *controlSocket;
    QTcpSocket  *reverseSocket;
    float        speed;
    double       position;
    double       initial_position;
    QUrl         url;
    AirplayEvent lastEvent;
    bool         stopped;
    bool         was_playing;
    bool         initialized;
};

class APHTTPRequest;

class MTV_PUBLIC MythAirplayServer : public ServerPool
{
    Q_OBJECT

  public:
    static bool Create(void);
    static void Cleanup(void);
    static MythAirplayServer *AirplaySharedInstance(void)
    { return gMythAirplayServer; }

    MythAirplayServer();

  private slots:
    void Start();
    void newConnection(QTcpSocket *client);
    void deleteConnection();
    void read(void);

  private:
    virtual ~MythAirplayServer(void);
    void Teardown(void);
    void HandleResponse(APHTTPRequest *req, QTcpSocket *socket);
    QByteArray StatusToString(int status);
    QString eventToString(AirplayEvent event);
    void GetPlayerStatus(bool &playing, float &speed,
                         double &position, double &duration,
                         QString &pathname);
    QString GetMacAddress();
    bool SendReverseEvent(QByteArray &session, AirplayEvent event);
    void SendResponse(QTcpSocket *socket,
                      int status, QByteArray header,
                      QByteArray content_type, QString body);

    void deleteConnection(QTcpSocket *socket);
    void DisconnectAllClients(const QByteArray &session);
    void StopSession(const QByteArray &session);
    void StartPlayback(const QString &pathname);
    void StopPlayback(void);
    void SeekPosition(uint64_t position);
    void PausePlayback(void);
    void UnpausePlayback(void);

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
    QString          m_pathname;

    //Authentication
    QString          m_nonce;

    //Incoming data
    QHash<QTcpSocket*, APHTTPRequest*> m_incoming;
};

#endif // MYTHAIRPLAYSERVER_H
