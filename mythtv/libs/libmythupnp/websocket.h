//////////////////////////////////////////////////////////////////////////////
// Program Name: websocket.h
// Created     : 18 Jan 2015
//
// Purpose     : Web Socket server
//
// Copyright (c) 2015 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "libmythbase/mthreadpool.h"
#include "libmythbase/serverpool.h"

#include "upnpexp.h"

#include <QRunnable>
#include <QSslConfiguration>
#include <QReadWriteLock>
#include <QEvent>
#include <QEventLoop>
#include <QTimer>

#include <cstdint>

/**
 * \class WebSocketServer
 *
 * \brief The WebSocket server, which listens for connections
 *
 * When WebSocketServer receives a connection it hands it off to a new instance
 * of WebSocketWorker which runs it it's own thread, WebSocketWorkerThread
 */
class UPNP_PUBLIC WebSocketServer : public ServerPool
{
    Q_OBJECT

  public:
    WebSocketServer();
    ~WebSocketServer() override;

    bool IsRunning(void) const
    {
        m_rwlock.lockForRead();
        bool tmp = m_running;
        m_rwlock.unlock();
        return tmp;
    }

  protected slots:
    void newTcpConnection(qintptr socket) override; // QTcpServer

  protected:
    mutable QReadWriteLock  m_rwlock;
    MThreadPool             m_threadPool;
    bool                    m_running {true}; // protected by m_rwlock

#ifndef QT_NO_OPENSSL
    QSslConfiguration       m_sslConfig;
#endif

  private:
     //void LoadSSLConfig();
};


/**
 * \class WebSocketFrame
 *
 * \brief A representation of a single WebSocket frame
 */
class WebSocketFrame
{
  public:
    WebSocketFrame()
    {
        m_mask.reserve(4);
    }

   ~WebSocketFrame()
    {
        m_payload.clear();
        m_mask.clear();
    }

    void reset(void)
    {
        m_finalFrame = false;
        m_payload.clear();
        m_payload.resize(128);
        m_payload.squeeze();
        m_mask.clear();
        m_payloadSize = 0;
        m_opCode = kOpTextFrame;
        m_fragmented = false;
    }

    enum OpCode : std::uint8_t
    {
        kOpContinuation = 0x0,
        kOpTextFrame    = 0x1,
        kOpBinaryFrame  = 0x2,
        // Reserved
        kOpClose        = 0x8,
        kOpPing         = 0x9,
        kOpPong         = 0xA
        // Reserved
    };

    bool       m_finalFrame  {false};
    QByteArray m_payload;
    uint64_t   m_payloadSize {0};
    OpCode     m_opCode      {kOpTextFrame};
    bool       m_isMasked    {false};
    QByteArray m_mask;
    bool       m_fragmented  {false};
};

class WebSocketWorker;

/**
 * \class WebSocketExtension
 *
 * \brief Base class for extensions
 *
 * Extensions enable different features to be operate via a websocket
 * connection without cluttering the general connection/parsing classes.
 *
 * Extensions can be registered and deregistered, so features can be enabled or
 * disabled easily. A frontend might offer different services to a backend for
 * example.
 */
class WebSocketExtension : public QObject
{
  Q_OBJECT

  public:
    WebSocketExtension() = default;;
    ~WebSocketExtension() override = default;

    virtual bool HandleTextFrame(const WebSocketFrame &/*frame*/) { return false; }
    virtual bool HandleBinaryFrame(const WebSocketFrame &/*frame*/) { return false; }

  signals:
    void SendTextMessage(const QString &);
    void SendBinaryMessage(const QByteArray &);
};

/**
 * \class WebSocketWorkerThread
 *
 * \brief The thread in which WebSocketWorker does it's thing
 */
class WebSocketWorkerThread : public QRunnable
{
  public:
    WebSocketWorkerThread(WebSocketServer &webSocketServer, qintptr sock,
                          PoolServerType type
#ifndef QT_NO_OPENSSL
                          , const QSslConfiguration& sslConfig
#endif
                        );
    ~WebSocketWorkerThread() override = default;

    void run(void) override; // QRunnable

  private:
    WebSocketServer  &m_webSocketServer;
    qintptr           m_socketFD;
    PoolServerType    m_connectionType;
#ifndef QT_NO_OPENSSL
    QSslConfiguration m_sslConfig;
#endif
};

/**
 * \class WebSocketWorker
 *
 * \brief Performs all the protocol-level work for a single websocket connection
 *
 * The worker is created by WebSocketServer after a raw socket is opened. It
 * runs in it's own thread, an instance of WebSocketServerThread.
 *
 * The worker parses the initial HTTP connection and upgrades the connection to
 * use the WebSocket protocol. It manages the heartbeat, parses and validates
 * frames, reconstitutes payloads from fragmented frames and handles other basic
 * protocol level tasks.
 *
 * When complete data frames have been received, it checks whether any
 * registered extensions wish to handle them. It is also responsible for
 * creating properly formatted, valid frames and transmitting them to the
 * client.
 */
class WebSocketWorker : public QObject
{
    Q_OBJECT

  public:

    /**
     * \param webSocketServer The parent server of this request
     * \param sock       The socket
     * \param type       The type of connection - Plain TCP or TLS?
     * \param sslConfig  The TLS (ssl) configuration (for TLS sockets)
     */
    WebSocketWorker(WebSocketServer &webSocketServer, qintptr sock,
                    PoolServerType type
#ifndef QT_NO_OPENSSL
                    , const QSslConfiguration& sslConfig
#endif
                    );
    ~WebSocketWorker() override;

    void Exec();

    enum ErrorCode : std::uint16_t
    {
        kCloseNormal        = 1000,
        kCloseGoingAway     = 1001,
        kCloseProtocolError = 1002,
        kCloseUnsupported   = 1003,
        // Reserved - 1004
        // The following codes, 1005 and 1006 should not go over the wire,
        // they are internal codes generated if the client has disconnected
        // without sending a Close event or without indicating a reason for
        // closure.
        kCloseNoStatus      = 1005,
        kCloseAbnormal      = 1006,
        kCloseBadData       = 1007,
        kClosePolicy        = 1008, // Message violates server policy and 1003/1009 don't apply
        kCloseTooLarge      = 1009, // Message is larger than we support (32KB)
        kCloseNoExtensions  = 1010, // CLIENT ONLY
        kCloseUnexpectedErr = 1011, // SERVER ONLY
        // Reserved - 1012-1014
        kCloseNoTLS         = 1012  // Connection closed because it must use TLS
        // Reserved
    };

  public slots:
    void doRead();
    void CloseConnection();

    void SendHeartBeat();
    bool SendText(const QString &message);
    bool SendText(const QByteArray &message);
    bool SendBinary(const QByteArray &data);

  protected:
    bool ProcessHandshake(QTcpSocket *socket); /// Returns false if an error occurs
    void ProcessFrames(QTcpSocket *socket); /// Returns false if an error occurs

    void HandleControlFrame(const WebSocketFrame &frame);
    void HandleDataFrame(const WebSocketFrame &frame);

    void HandleCloseConnection(const QByteArray &payload);

    static QByteArray CreateFrame(WebSocketFrame::OpCode type, const QByteArray &payload);

    bool SendFrame(const QByteArray &frame);
    bool SendPing(const QByteArray &payload);
    bool SendPong(const QByteArray &payload);
    bool SendClose(ErrorCode errCode, const QString &message = QString());

    void SetupSocket();
    void CleanupSocket();

    void RegisterExtension(WebSocketExtension *extension);
    void DeregisterExtension(WebSocketExtension *extension);

    QEventLoop     *m_eventLoop    {nullptr};
    WebSocketServer &m_webSocketServer;
    qintptr        m_socketFD;
    QTcpSocket    *m_socket        {nullptr};
    PoolServerType m_connectionType;

    // True if we've successfully upgraded from HTTP
    bool           m_webSocketMode {false};
    WebSocketFrame m_readFrame;

    uint8_t m_errorCount           {0};
    bool    m_isRunning            {false};

    QTimer *m_heartBeat            {nullptr};

#ifndef QT_NO_OPENSSL
    QSslConfiguration       m_sslConfig;
#endif

    bool    m_fuzzTesting          {false};

    QList<WebSocketExtension *> m_extensions;
};

#endif // WEBSOCKET_H
