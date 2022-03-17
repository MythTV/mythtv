#ifndef MYTHHTTPSOCKET_H
#define MYTHHTTPSOCKET_H
// Qt
#include <QObject>
#include <QTimer>
#include <QAbstractSocket>
#include <QElapsedTimer>

// MythTV
#include "libmythbase/http/mythhttpparser.h"
#include "libmythbase/http/mythhttptypes.h"

class QTcpSocket;
class QSslSocket;
class MythWebSocket;
class MythWebSocketEvent;

class MythHTTPSocket : public QObject
{
    Q_OBJECT

  signals:
    void Finish();
    void UpdateServices(const HTTPServices& Services);
    void ThreadUpgraded(QThread* Thread);

  public slots:
    void PathsChanged     (const QStringList&  Paths);
    void HandlersChanged  (const HTTPHandlers& Handlers);
    void ServicesChanged  (const HTTPServices& Services);
    void HostsChanged     (const QStringList&  Hosts);
    void OriginsChanged   (const QStringList&  Origins);
    void NewTextMessage   (const StringPayload& Text);
    void NewRawTextMessage(const DataPayloads& Payloads);
    static void NewBinaryMessage (const DataPayloads& Payloads);

  public:
    explicit MythHTTPSocket(qintptr Socket, bool SSL, MythHTTPConfig Config);
   ~MythHTTPSocket() override;
    void Respond(const HTTPResponse& Response);
    static void RespondDirect(qintptr Socket, const HTTPResponse& Response, const MythHTTPConfig& Config);

  protected slots:
    void Disconnected();
    void Timeout();
    void Read();
    void Stop();
    void Write(int64_t Written = 0);
    void Error(QAbstractSocket::SocketError Error);

  private:
    Q_DISABLE_COPY(MythHTTPSocket)
    void SetupWebSocket();

    qintptr         m_socketFD       { 0 };
    MythHTTPConfig  m_config;
    HTTPServicePtrs m_activeServices;
    bool            m_stopping       { false };
    QTcpSocket*     m_socket         { nullptr };
    MythWebSocket*  m_websocket      { nullptr };
    QString         m_peer;
    QTimer          m_timer;
    MythHTTPParser  m_parser;
    HTTPQueue       m_queue;
    int64_t         m_totalToSend    { 0 };
    int64_t         m_totalWritten   { 0 };
    int64_t         m_totalSent      { 0 };
    QElapsedTimer   m_writeTime;
    HTTPData        m_writeBuffer    { nullptr };
    MythHTTPConnection m_nextConnection { HTTPConnectionClose };
    MythSocketProtocol m_protocol    { ProtHTTP };
    // WebSockets only
    bool                m_testSocket     { false };
    MythWebSocketEvent* m_websocketevent { nullptr };
};

#endif
