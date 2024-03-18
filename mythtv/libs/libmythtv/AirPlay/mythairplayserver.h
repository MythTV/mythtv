#ifndef MYTHAIRPLAYSERVER_H
#define MYTHAIRPLAYSERVER_H

#include <QObject>
#include <QRecursiveMutex>
#include <QUrl>
#include <cstdint>     // for uintxx_t

#include "libmythbase/serverpool.h"
#include "libmythui/mythmainwindow.h"

#include "libmythtv/mythtvexp.h"

class QTimer;
class MThread;
class BonjourRegister;

static constexpr int AIRPLAY_PORT_RANGE { 100 };
static constexpr size_t AIRPLAY_HARDWARE_ID_SIZE { 6 };
QString     AirPlayHardwareId(void);
QString     GenerateNonce(void);
QByteArray  DigestMd5Response(const QString& response, const QString& option,
                              const QString& nonce, const QString& password,
                              QByteArray &auth);

enum AirplayEvent : std::int8_t
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
    AirplayConnection() = default;
    ~AirplayConnection()
    {
        UnRegister();
    }
    void UnRegister(void)
    {
        if (m_notificationid > 0)
        {
            GetNotificationCenter()->UnRegister(this, m_notificationid);
            m_notificationid = -1;
        }
    }
    QTcpSocket  *m_controlSocket    {nullptr};
    QTcpSocket  *m_reverseSocket    {nullptr};
    float        m_speed            {1.0F};
    double       m_position         {0.0};
//  double       m_initial_position {-1.0};
    QUrl         m_url;
    AirplayEvent m_lastEvent        {AP_EVENT_NONE};
    bool         m_stopped          {false};
    bool         m_was_playing      {false};
    bool         m_initialized      {false};
    bool         m_photos           {false};
    // Notification
    int          m_notificationid   {-1};
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

    MythAirplayServer() :
        m_lock(new QRecursiveMutex())
        {}

  private slots:
    void Start();
    void Stop();
    void newAirplayConnection(QTcpSocket *client);
    void deleteConnection();
    void read(void);
    void timeout(void);

  private:
    ~MythAirplayServer(void) override;
    void Teardown(void);
    void HandleResponse(APHTTPRequest *req, QTcpSocket *socket);
    static QByteArray StatusToString(uint16_t status);
    static QString eventToString(AirplayEvent event);
    static void GetPlayerStatus(bool &playing, float &speed,
                                double &position, double &duration,
                                QString &pathname);
    static QString GetMacAddress();
    bool SendReverseEvent(QByteArray &session, AirplayEvent event);
    void SendResponse(QTcpSocket *socket,
                      uint16_t status, const QByteArray& header,
                      const QByteArray& content_type, const QString& body);

    void deleteConnection(QTcpSocket *socket);
    void DisconnectAllClients(const QByteArray &session);
    void StopSession(const QByteArray &session);
    void StartPlayback(const QString &pathname);
    void StopPlayback(void);
    void SeekPosition(uint64_t position);
    void PausePlayback(void);
    void UnpausePlayback(void);
    void HideAllPhotos(void);

    // Globals
    static MythAirplayServer *gMythAirplayServer;
    static QRecursiveMutex   *gMythAirplayServerMutex;
    static MThread           *gMythAirplayServerThread;

    // Members
    QString          m_name          {"MythTV"};
    BonjourRegister *m_bonjour       {nullptr};
    bool             m_valid         {false};
    QRecursiveMutex *m_lock          {nullptr};
    int              m_setupPort     {5100};
    QList<QTcpSocket*> m_sockets;
    QHash<QByteArray,AirplayConnection> m_connections;
    QString          m_pathname;

    //Authentication
    QString          m_nonce;

    //Incoming data
    QHash<QTcpSocket*, APHTTPRequest*> m_incoming;

    // Bonjour Service re-advertising
    QTimer         *m_serviceRefresh {nullptr};
};

#endif // MYTHAIRPLAYSERVER_H
