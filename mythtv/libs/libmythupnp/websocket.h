//////////////////////////////////////////////////////////////////////////////
// Program Name: websocket.h
// Created     : 18 Jan 2015
//
// Purpose     : Web Socket server
//
// Copyright (c) 2015 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include "serverpool.h"
#include "upnpexp.h"
#include "mthreadpool.h"

#include <QRunnable>
#include <QSslConfiguration>
#include <QReadWriteLock>
#include <QEvent>
#include <QEventLoop>
#include <QTimer>

#include <stdint.h>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// WebSocketServer Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC WebSocketServer : public ServerPool
{
    Q_OBJECT

  public:
    WebSocketServer();
    virtual ~WebSocketServer();

    bool IsRunning(void) const
    {
        m_rwlock.lockForRead();
        bool tmp = m_running;
        m_rwlock.unlock();
        return tmp;
    }

  protected slots:
    virtual void newTcpConnection(qt_socket_fd_t socket); // QTcpServer

  protected:
    mutable QReadWriteLock  m_rwlock;
    MThreadPool             m_threadPool;
    bool                    m_running; // protected by m_rwlock

    QSslConfiguration       m_sslConfig;

  private:
     //void LoadSSLConfig();
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// WebSocketWorkerThread Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class WebSocketWorkerThread : public QRunnable
{
  public:
    WebSocketWorkerThread(WebSocketServer &webSocketServer, qt_socket_fd_t sock,
                          PoolServerType type, QSslConfiguration sslConfig);
    virtual ~WebSocketWorkerThread();

    virtual void run(void);

  private:
    WebSocketServer  &m_webSocketServer;
    qt_socket_fd_t    m_socketFD;
    PoolServerType    m_connectionType;
    QSslConfiguration m_sslConfig;
};

class WebSocketFrame
{
  public:
    WebSocketFrame() : finalFrame(false), payloadSize(0), opCode(kOpTextFrame),
                       isMasked(false), fragmented(false)
    {
       mask.reserve(4);
    }

   ~WebSocketFrame()
    {
        payload.clear();
        mask.clear();
    }

    void reset(void)
    {
        finalFrame = false;
        payload.clear();
        payload.resize(128);
        payload.squeeze();
        mask.clear();
        payloadSize = 0;
        opCode = kOpTextFrame;
        fragmented = false;
    }

    typedef enum OpCodes
    {
        kOpContinuation = 0x0,
        kOpTextFrame    = 0x1,
        kOpBinaryFrame  = 0x2,
        // Reserved
        kOpClose        = 0x8,
        kOpPing         = 0x9,
        kOpPong         = 0xA
        // Reserved
    } OpCode;

    bool finalFrame;
    QByteArray payload;
    uint64_t payloadSize;
    OpCode opCode;
    bool isMasked;
    QByteArray mask;
    bool fragmented;
};

class WebSocketWorker : public QObject
{
    Q_OBJECT

  public:

    /**
     * \param webSocketServer The parent server of this request
     * \param sock       The socket
     * \param type       The type of connection - Plain TCP or TLS?
     * \param tlsConfig  The TLS (ssl) configuration (for TLS sockets)
     */
    WebSocketWorker(WebSocketServer &webSocketServer, qt_socket_fd_t sock,
                    PoolServerType type, QSslConfiguration sslConfig);
    virtual ~WebSocketWorker();

    void Exec();

    typedef enum ErrorCodes
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
    } ErrorCode;

  public slots:
    void doRead();
    void CloseConnection();

    void SendHeartBeat();

  protected:
    bool ProcessHandshake(QTcpSocket *); /// Returns false if an error occurs
    void ProcessFrames(QTcpSocket *); /// Returns false if an error occurs

    void HandleControlFrame(const WebSocketFrame &frame);
    void HandleDataFrame(const WebSocketFrame &frame);

    void HandleCloseConnection(const QByteArray &payload);

    QByteArray CreateFrame(WebSocketFrame::OpCode type, const QByteArray &payload);

    bool SendFrame(const QByteArray &frame);
    bool SendText(const QString &message);
    bool SendText(const QByteArray &message);
    bool SendBinary(const QByteArray &data);
    bool SendPing(const QByteArray &payload);
    bool SendPong(const QByteArray &payload);
    bool SendClose(ErrorCode errCode, const QString &message = QString());

    void customEvent(QEvent *event);

    void SetupSocket();
    void CleanupSocket();

    QEventLoop *m_eventLoop;
    WebSocketServer &m_webSocketServer;
    qt_socket_fd_t m_socketFD;
    QTcpSocket *m_socket;
    PoolServerType m_connectionType;

    bool m_webSocketMode; // True if we've successfully upgraded from HTTP
    WebSocketFrame m_readFrame;

    uint8_t m_errorCount;
    bool m_isRunning;

    QTimer *m_heartBeat;

    QSslConfiguration       m_sslConfig;

    bool m_fuzzTesting;
};

#endif
