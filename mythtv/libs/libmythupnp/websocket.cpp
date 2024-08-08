// Own header
#include "websocket.h"

// C++
#include <algorithm>
#include <chrono>
#include <cmath>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/stringutil.h"

#include "websocket_extensions/websocket_mythevent.h"

// QT headers
#include <QCryptographicHash>
#include <QSslCipher>
#include <QTcpSocket>
#include <QThread>
#include <QtGlobal>

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WebSocketServer::WebSocketServer() :
    m_threadPool("WebSocketServerPool")
{
    setObjectName("WebSocketServer");
    // Number of connections processed concurrently
    int maxThreads = std::max(QThread::idealThreadCount() * 2, 2); // idealThreadCount can return -1
    // Don't allow more connections than we can process, it will simply cause
    // browsers to stall
    setMaxPendingConnections(maxThreads);
    m_threadPool.setMaxThreadCount(maxThreads);

    LOG(VB_HTTP, LOG_NOTICE, QString("WebSocketServer(): Max Thread Count %1")
                                .arg(m_threadPool.maxThreadCount()));
}

WebSocketServer::~WebSocketServer()
{
    m_rwlock.lockForWrite();
    m_running = false;
    m_rwlock.unlock();

    m_threadPool.Stop();
}

void WebSocketServer::newTcpConnection(qintptr socket)
{

    PoolServerType type = kTCPServer;
    auto *server = qobject_cast<PrivTcpServer *>(QObject::sender());
    if (server)
        type = server->GetServerType();

    m_threadPool.startReserved(
        new WebSocketWorkerThread(*this, socket, type
#ifndef QT_NO_OPENSSL
               , m_sslConfig
#endif
               ),
        QString("WebSocketServer%1").arg(socket));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WebSocketWorkerThread::WebSocketWorkerThread(WebSocketServer& webSocketServer,
                                 qintptr sock, PoolServerType type
#ifndef QT_NO_OPENSSL
                                 , const QSslConfiguration& sslConfig
#endif
                                 )
                :
                  m_webSocketServer(webSocketServer), m_socketFD(sock),
                  m_connectionType(type)
#ifndef QT_NO_OPENSSL
                  , m_sslConfig(sslConfig)
#endif
{
}

void WebSocketWorkerThread::run(void)
{
    auto *worker = new WebSocketWorker(m_webSocketServer, m_socketFD,
                                                  m_connectionType
#ifndef QT_NO_OPENSSL
                                                  , m_sslConfig
#endif
                                                  );
    worker->Exec();

    worker->deleteLater();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WebSocketWorker::WebSocketWorker(WebSocketServer& webSocketServer,
                                 qintptr sock, PoolServerType type
#ifndef QT_NO_OPENSSL
                                 , const QSslConfiguration& sslConfig
#endif
                                 )
                : m_eventLoop(new QEventLoop()),
                  m_webSocketServer(webSocketServer),
                  m_socketFD(sock), m_connectionType(type),
                  m_heartBeat(new QTimer())
#ifndef QT_NO_OPENSSL
                  , m_sslConfig(sslConfig)
#endif
{
    setObjectName(QString("WebSocketWorker(%1)")
                        .arg(m_socketFD));
    LOG(VB_HTTP, LOG_INFO, QString("WebSocketWorker(%1): New connection")
                                        .arg(m_socketFD));

    // For now, until it's refactored, register the only extension here
    RegisterExtension(new WebSocketMythEvent());

    SetupSocket();

    m_isRunning = true;
}

WebSocketWorker::~WebSocketWorker()
{
    CleanupSocket();

    // If extensions weren't deregistered then it's our responsibility to
    // delete them
    while (!m_extensions.isEmpty())
    {
        WebSocketExtension *extension = m_extensions.takeFirst();
        extension->deleteLater();
        extension = nullptr;
    }

    m_eventLoop->deleteLater();
    m_eventLoop = nullptr;
    delete m_heartBeat;
}

void WebSocketWorker::Exec()
{
    m_eventLoop->exec();
}

void WebSocketWorker::CloseConnection()
{
    blockSignals(true);
    m_isRunning = false;
    LOG(VB_HTTP, LOG_INFO, "WebSocketWorker::CloseConnection()");
    if (m_eventLoop)
        m_eventLoop->exit();
}

void WebSocketWorker::SetupSocket()
{
    if (m_connectionType == kSSLServer)
    {

#ifndef QT_NO_OPENSSL
        auto *pSslSocket = new QSslSocket();
        if (pSslSocket->setSocketDescriptor(m_socketFD)
           && gCoreContext->CheckSubnet(pSslSocket))
        {
            pSslSocket->setSslConfiguration(m_sslConfig);
            pSslSocket->startServerEncryption();
            if (pSslSocket->waitForEncrypted(5000))
            {
                LOG(VB_HTTP, LOG_INFO, "SSL Handshake occurred, connection encrypted");
                LOG(VB_HTTP, LOG_INFO, QString("Using %1 cipher").arg(pSslSocket->sessionCipher().name()));
            }
            else
            {
                LOG(VB_HTTP, LOG_WARNING, "SSL Handshake FAILED, connection terminated");
                delete pSslSocket;
                pSslSocket = nullptr;
            }
        }
        else
        {
            delete pSslSocket;
            pSslSocket = nullptr;
        }

        if (pSslSocket)
            m_socket = pSslSocket;
        else
            return;
#else
        return;
#endif
    }
    else // Plain old unencrypted socket
    {
        m_socket = new QTcpSocket();
        m_socket->setSocketDescriptor(m_socketFD);
        if (!gCoreContext->CheckSubnet(m_socket))
        {
            delete m_socket;
            m_socket = nullptr;
            return;
        }

    }

    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, QVariant(1));

    connect(m_socket, &QIODevice::readyRead, this, &WebSocketWorker::doRead);
    connect(m_socket, &QAbstractSocket::disconnected, this, &WebSocketWorker::CloseConnection);

    // Setup heartbeat
    m_heartBeat->setInterval(20s);
    m_heartBeat->setSingleShot(false);
    connect(m_heartBeat, &QTimer::timeout, this, &WebSocketWorker::SendHeartBeat);
}

void WebSocketWorker::CleanupSocket()
{
    if (m_socket->error() != QAbstractSocket::UnknownSocketError)
    {
        LOG(VB_HTTP, LOG_WARNING, QString("WebSocketWorker(%1): Error %2 (%3)")
                                   .arg(m_socketFD)
                                   .arg(m_socket->errorString())
                                   .arg(m_socket->error()));
    }

    std::chrono::milliseconds writeTimeout = 5s;
    // Make sure any data in the buffer is flushed before the socket is closed
    while (m_webSocketServer.IsRunning() &&
           m_socket->isValid() &&
           m_socket->state() == QAbstractSocket::ConnectedState &&
           m_socket->bytesToWrite() > 0)
    {
        LOG(VB_HTTP, LOG_DEBUG, QString("WebSocketWorker(%1): "
                                        "Waiting for %2 bytes to be written "
                                        "before closing the connection.")
                                            .arg(m_socketFD)
                                            .arg(m_socket->bytesToWrite()));

        // If the client stops reading for longer than 'writeTimeout' then
        // stop waiting for them. We can't afford to leave the socket
        // connected indefinitely, it could be used by another client.
        //
        // NOTE: Some clients deliberately stall as a way of 'pausing' A/V
        // streaming. We should create a new server extension or adjust the
        // timeout according to the User-Agent, instead of increasing the
        // standard timeout. However we should ALWAYS have a timeout.
        if (!m_socket->waitForBytesWritten(writeTimeout.count()))
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("WebSocketWorker(%1): "
                                         "Timed out waiting to write bytes to "
                                         "the socket, waited %2 seconds")
                                            .arg(m_socketFD)
                                            .arg(writeTimeout.count() / 1000));
            break;
        }
    }

    if (m_socket->bytesToWrite() > 0)
    {
        LOG(VB_HTTP, LOG_WARNING, QString("WebSocketWorker(%1): "
                                          "Failed to write %2 bytes to "
                                          "socket, (%3)")
                                            .arg(m_socketFD)
                                            .arg(m_socket->bytesToWrite())
                                            .arg(m_socket->errorString()));
    }

    LOG(VB_HTTP, LOG_INFO, QString("WebSocketWorker(%1): Connection %2 closed.")
                                        .arg(m_socketFD)
                                        .arg(m_socket->socketDescriptor()));

    m_socket->close();
    m_socket->deleteLater();
    m_socket = nullptr;
}

void WebSocketWorker::doRead()
{
    if (!m_isRunning)
        return;

    if (!m_webSocketServer.IsRunning() || !m_socket->isOpen())
    {
        CloseConnection();
        return;
    }

    if (m_webSocketMode)
    {
        ProcessFrames(m_socket);
    }
    else
    {
        if (!ProcessHandshake(m_socket))
            SendClose(kCloseProtocolError);
    }

    if (!m_webSocketMode)
    {
        LOG(VB_HTTP, LOG_WARNING, "WebSocketServer: Timed out waiting for connection upgrade");
        CloseConnection();
    }
}

bool WebSocketWorker::ProcessHandshake(QTcpSocket *socket)
{
    QByteArray line;

    // Minimum length of the request line is 16
    // i.e. "GET / HTTP/1.1\r\n"
    while (socket->bytesAvailable() < 16)
    {
        if (!socket->waitForReadyRead(5000)) // 5 second timeout
            return false;
    }

    // Set a maximum length for a request line of 255 bytes, it's an
    // arbitrary limit but a reasonable one for now
    line = socket->readLine(255).trimmed(); // Strip newline
    if (line.isEmpty())
        return false;

    QStringList tokens = QString(line).split(' ', Qt::SkipEmptyParts);
    if (tokens.length() != 3) // Anything but 3 is invalid - {METHOD} {HOST/PATH} {PROTOCOL}
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessHandshake() - Invalid number of tokens in Request line");
        return false;
    }

    if (tokens[0] != "GET") // RFC 6455 - Request method MUST be GET
    {
        LOG(VB_GENERAL, LOG_ERR, QString("WebSocketWorker::ProcessHandshake() - Invalid method: %1").arg(tokens[0]));
        return false;
    }

    const QString& path = tokens[1];

    if (path.contains('#')) // RFC 6455 - Fragments MUST NOT be used
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessHandshake() - Path contains illegal fragment");
        return false;
    }

    if (!tokens[2].startsWith("HTTP/")) // Must be HTTP
    {
        LOG(VB_GENERAL, LOG_ERR, QString("WebSocketWorker::ProcessHandshake() - Invalid protocol: %1").arg(tokens[2]));
        return false;
    }

    QString versionStr = tokens[2].section('/', 1, 1);

    int majorVersion = versionStr.section('.', 0, 0).toInt();
    int minorVersion = versionStr.section('.', 1, 1).toInt();

    if ((majorVersion < 1) || (minorVersion < 1)) // RFC 6455 - HTTP version MUST be at least 1.1
    {
        LOG(VB_GENERAL, LOG_ERR, QString("WebSocketWorker::ProcessHandshake() - Invalid HTTP version: %1.%2").arg(majorVersion).arg(minorVersion));
        return false;
    }

    // Read Header
    line = socket->readLine(2000).trimmed(); // 2KB Maximum

    QMap<QString, QString> requestHeaders;
    while (!line.isEmpty())
    {
        QString strLine = QString(line);
        QString sName  = strLine.section( ':', 0, 0 ).trimmed();
        QString sValue = strLine.section( ':', 1 ).trimmed();
        sValue.replace(" ",""); // Remove all internal whitespace

        if (!sName.isEmpty() && !sValue.isEmpty())
            requestHeaders.insert(sName.toLower(), sValue);

        line = socket->readLine(2000).trimmed(); // 2KB Maximum
    }

    // Dump request header
    QMap<QString, QString>::iterator it;
    for ( it = requestHeaders.begin();
          it != requestHeaders.end();
          ++it )
    {
        LOG(VB_HTTP, LOG_INFO, QString("(Request Header) %1: %2")
                                        .arg(it.key(), *it));
    }

    if (!requestHeaders.contains("connection")) // RFC 6455 - 1.3. Opening Handshake
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessHandshake() - Missing 'Connection' header");
        return false;
    }

    QStringList connectionValues = requestHeaders["connection"].split(',', Qt::SkipEmptyParts);
    if (!connectionValues.contains("Upgrade", Qt::CaseInsensitive)) // RFC 6455 - 1.3. Opening Handshake
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessHandshake() - Invalid 'Connection' header");
        return false;
    }

    if (!requestHeaders.contains("upgrade") ||
        requestHeaders["upgrade"].toLower() != "websocket") // RFC 6455 - 1.3. Opening Handshake
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessHandshake() - Missing or invalid 'Upgrade' header");
        return false;
    }

    if (!requestHeaders.contains("sec-websocket-version")) // RFC 6455 - 1.3. Opening Handshake
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessHandshake() - Missing 'Sec-WebSocket-Version' header");
        return false;
    }

    if (requestHeaders["sec-websocket-version"] != "13") // RFC 6455 - 1.3. Opening Handshake
    {
        LOG(VB_GENERAL, LOG_ERR, QString("WebSocketWorker::ProcessHandshake() - Unsupported WebSocket protocol "
                                      "version. We speak '13' the client speaks '%1'")
                                            .arg(requestHeaders["sec-websocket-version"]));
        return false;
    }

    if (!requestHeaders.contains("sec-websocket-key") ||
        QByteArray::fromBase64(requestHeaders["sec-websocket-key"].toLatin1()).length() != 16) // RFC 6455 - 1.3. Opening Handshake
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessHandshake() - Missing or invalid 'sec-websocket-key' header");
        return false;
    }

    // If we're running the Autobahn fuzz/unit tester then we need to disable
    // the heartbeat and 'echo' text/binary frames back to the sender
    if (requestHeaders.contains("user-agent") &&
        requestHeaders["user-agent"].contains("AutobahnTestSuite"))
        m_fuzzTesting = true;

    QString key = requestHeaders["sec-websocket-key"];
    const QString magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // Response token - Watch very closely, there's a lot happening here
    QString acceptToken = QCryptographicHash::hash(key.append(magicString).toUtf8(),
                                                   QCryptographicHash::Sha1).toBase64();

    QMap<QString, QString> responseHeaders;
    responseHeaders.insert("Connection", "Upgrade");
    responseHeaders.insert("Upgrade", "websocket");
    responseHeaders.insert("Sec-WebSocket-Accept", acceptToken);

    socket->write("HTTP/1.1 101 Switching Protocols\r\n");

    QString header("%1: %2\r\n");
    for (it = responseHeaders.begin(); it != responseHeaders.end(); ++it)
    {
        socket->write(header.arg(it.key(), *it).toLatin1());
        LOG(VB_HTTP, LOG_INFO, QString("(Response Header) %1: %2")
                                        .arg(it.key(), *it));
    }

    socket->write("\r\n");
    socket->waitForBytesWritten();

    m_webSocketMode = true;
    // Start the heart beat
    if (!m_fuzzTesting)
        m_heartBeat->start();

    return true;
}

void WebSocketWorker::ProcessFrames(QTcpSocket *socket)
{
    while (m_isRunning && socket && socket->bytesAvailable() >= 2) // No header? Return and wait for more
    {
        uint8_t headerSize = 2; // Smallest possible header size is 2 bytes, greatest is 14 bytes

        QByteArray header = socket->peek(headerSize); // Read header to establish validity and size of frame

        WebSocketFrame frame;
        // FIN
        frame.m_finalFrame = (bool)(header[0] & 0x80);
        // Reserved bits
        if (header.at(0) & 0x70)
        {
            LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessFrames() "
                                     "- Invalid data in reserved fields");
            SendClose(kCloseProtocolError, "Invalid data in reserved fields");
            return;
        }
        // Operation code
        uint8_t opCode = (header.at(0) & 0xF);
        if ((opCode > WebSocketFrame::kOpBinaryFrame &&
             opCode < WebSocketFrame::kOpClose) ||
             (opCode > WebSocketFrame::kOpPong))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("WebSocketWorker::ProcessFrames() "
                                             "- Invalid OpCode (%1)")
                                            .arg(QString::number(opCode, 16)));
            SendClose(kCloseProtocolError, "Invalid OpCode");
            return;
        }
        frame.m_opCode = (WebSocketFrame::OpCode)opCode;
        frame.m_isMasked = (((header.at(1) >> 7) & 0xFE) != 0);

        if (frame.m_isMasked)
            headerSize += 4; // Add 4 bytes for the mask

        frame.m_payloadSize = (header.at(1) & 0x7F);
        // Handle 16 or 64bit payload size headers
        if (frame.m_payloadSize >= 126)
        {
            uint8_t payloadHeaderSize = 2; // 16bit payload size
            if (frame.m_payloadSize == 127)
                payloadHeaderSize = 8; // 64bit payload size

            headerSize += payloadHeaderSize; // Add bytes for extended payload size

            if (socket->bytesAvailable() < headerSize)
                return; // Return and wait for more

            header = socket->peek(headerSize); // Peek the entire header
            QByteArray payloadHeader = header.mid(2,payloadHeaderSize);
            frame.m_payloadSize = 0;
            for (int i = 0; i < payloadHeaderSize; i++)
            {
                frame.m_payloadSize |= ((uint64_t)payloadHeader.at(i) << ((payloadHeaderSize - (i + 1)) * 8));
            }
        }
        else
        {
            if (socket->bytesAvailable() < headerSize)
                return; // Return and wait for more
            header = socket->peek(headerSize); // Peek the entire header including mask
        }

        while ((uint64_t)socket->bytesAvailable() < (frame.m_payloadSize + header.length()))
        {
            if (!socket->waitForReadyRead(2000)) // Wait 2 seconds for the next chunk of the frame
            {

                m_errorCount++;

                if (m_errorCount == 5)
                {
                    LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessFrames() - Timed out waiting for rest of frame to arrive.");
                    SendClose(kCloseBadData);
                }
                return;
            }
        }

        if (frame.m_opCode == WebSocketFrame::kOpContinuation)
             m_readFrame.m_payloadSize += frame.m_payloadSize;

        LOG(VB_HTTP, LOG_DEBUG, QString("Read Header: %1").arg(QString(header.toHex())));
        LOG(VB_HTTP, LOG_DEBUG, QString("Final Frame: %1").arg(frame.m_finalFrame ? "Yes" : "No"));
        LOG(VB_HTTP, LOG_DEBUG, QString("Op Code: %1").arg(QString::number(frame.m_opCode)));
        LOG(VB_HTTP, LOG_DEBUG, QString("Payload Size: %1 Bytes").arg(QString::number(frame.m_payloadSize)));
        LOG(VB_HTTP, LOG_DEBUG, QString("Total Payload Size: %1 Bytes").arg(QString::number( m_readFrame.m_payloadSize)));

        if (!m_fuzzTesting &&
            frame.m_payloadSize > std::pow(2,20)) // Set 1MB limit on payload per frame
        {
            LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessFrames() - Frame payload larger than limit of 1MB");
            SendClose(kCloseTooLarge, "Frame payload larger than limit of 1MB");
            return;
        }

        if (!m_fuzzTesting &&
            m_readFrame.m_payloadSize > std::pow(2,22)) // Set 4MB limit on total payload
        {
            LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::ProcessFrames() - Total payload larger than limit of 4MB");
            SendClose(kCloseTooLarge, "Total payload larger than limit of 4MB");
            return;
        }

        socket->read(headerSize); // Discard header from read buffer

        frame.m_payload = socket->read(frame.m_payloadSize);

        // Unmask payload
        if (frame.m_isMasked)
        {
            frame.m_mask = header.right(4);
            for (uint i = 0; i < frame.m_payloadSize; i++)
                frame.m_payload[i] = frame.m_payload.at(i) ^ frame.m_mask[i % 4];
        }

        if (m_readFrame.m_fragmented
            && frame.m_opCode > WebSocketFrame::kOpContinuation
            && frame.m_opCode < WebSocketFrame::kOpClose)
        {
            LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker - Incomplete multi-part frame? Expected continuation.");
            SendClose(kCloseProtocolError, "Incomplete multi-part frame? Expected continuation.");
            return;
        }

        // Check control frame validity
        if (frame.m_opCode >= 0x08)
        {
            if (!frame.m_finalFrame)
            {
                SendClose(kCloseProtocolError, "Control frames MUST NOT be fragmented");
                return;
            }
            if (frame.m_payloadSize > 125)
            {
                SendClose(kCloseProtocolError, "Control frames MUST NOT have payload greater than 125 bytes");
                return;
            }
        }

        switch (frame.m_opCode)
        {
            case WebSocketFrame::kOpContinuation:
                if (!m_readFrame.m_fragmented)
                {
                    LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker - Received Continuation Frame out of sequence");
                    SendClose(kCloseProtocolError, "Received Continuation Frame out of sequence");
                    return;
                }

                m_readFrame.m_payload.append(frame.m_payload);

                if (m_readFrame.m_fragmented && frame.m_finalFrame)
                {
                    m_readFrame.m_finalFrame = true;
                    frame = m_readFrame;
                    // Fall through to appropriate handler for complete payload
                }
                else
                {
                    break;
                }
                [[fallthrough]];
            case WebSocketFrame::kOpTextFrame:
            case WebSocketFrame::kOpBinaryFrame:
                HandleDataFrame(frame);
                break;
            case WebSocketFrame::kOpPing:
                SendPong(frame.m_payload);
                break;
            case WebSocketFrame::kOpPong:
                break;
            case WebSocketFrame::kOpClose:
                if (!frame.m_finalFrame)
                    SendClose(kCloseProtocolError, "Control frames MUST NOT be fragmented");
                else
                    HandleCloseConnection(frame.m_payload);
                break;
            default:
                LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker - Received Unknown Frame Type");
                break;
        }

        frame.reset();
    }
}

void WebSocketWorker::HandleControlFrame(const WebSocketFrame &/*frame*/)
{
}

void WebSocketWorker::HandleDataFrame(const WebSocketFrame &frame)
{
    if (frame.m_finalFrame)
    {
        switch (frame.m_opCode)
        {
            case WebSocketFrame::kOpTextFrame :
                if (!StringUtil::isValidUTF8(frame.m_payload))
                {
                    LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker - Message is not valid UTF-8");
                    SendClose(kCloseBadData, "Message is not valid UTF-8");
                    return;
                }
                // For Debugging and fuzz testing
                if (m_fuzzTesting)
                    SendText(frame.m_payload);
                for (auto *const extension : std::as_const(m_extensions))
                {
                    if (extension->HandleTextFrame(frame))
                        break;
                }
                break;
            case WebSocketFrame::kOpBinaryFrame :
                if (m_fuzzTesting)
                    SendBinary(frame.m_payload);
                for (auto *const extension : std::as_const(m_extensions))
                {
                    if (extension->HandleBinaryFrame(frame))
                        break;
                }
                break;
            default:
                break;
        }
        m_readFrame.reset();
    }
    else
    {
        // Start of new fragmented frame
        m_readFrame.reset();
        m_readFrame.m_opCode = frame.m_opCode;
        m_readFrame.m_payloadSize = frame.m_payloadSize;
        m_readFrame.m_payload = frame.m_payload;
        m_readFrame.m_fragmented = true;
        m_readFrame.m_finalFrame = false;
    }
}

void WebSocketWorker::HandleCloseConnection(const QByteArray &payload)
{
    uint16_t code = kCloseNormal;

    if (payload.length() == 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker - Invalid close payload");
        SendClose(kCloseProtocolError, "Invalid close payload");
        return;
    }

    if (payload.length() >= 2)
    {
        code = (payload[0] << 8);
        code |= payload[1] & 0xFF;
    }

    if ((code < 1000) ||
        ((code > 1003) && (code < 1007)) ||
        ((code > 1011) && (code < 3000)) ||
        (code > 4999))
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker - Invalid close code received");
        SendClose(kCloseProtocolError, "Invalid close code");
        return;
    }

    QString closeMessage;
    if (payload.length() > 2)
    {
        QByteArray messageBytes = payload.mid(2);
        if (!StringUtil::isValidUTF8(messageBytes))
        {
            LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker - Message is not valid UTF-8");
            SendClose(kCloseBadData, "Message is not valid UTF-8");
            return;
        }
        closeMessage = QString(messageBytes);
    }

    LOG(VB_HTTP, LOG_INFO, QString("WebSocketWorker - Received CLOSE frame - [%1] %2")
                                    .arg(QString::number(code), closeMessage));
    SendClose((ErrorCode)code);
}

QByteArray WebSocketWorker::CreateFrame(WebSocketFrame::OpCode type,
                                        const QByteArray &payload)
{
    QByteArray frame;

    int payloadSize = payload.length();

    if (payloadSize >= std::pow(2,64))
    {
        LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::CreateFrame() - Payload "
                              "exceeds the allowed size for a single frame");
        return frame;
    }

    if (type >= 0x08)
    {
        //if (!m_finalFrame)
        //   SendClose(kCloseProtocolError, "Control frames MUST NOT be fragmented");
        if (payloadSize > 125)
        {
            LOG(VB_GENERAL, LOG_ERR, "WebSocketWorker::CreateFrame() - Control frames MUST NOT have payload greater than 125bytes");
            return frame;
        }
    }

    uint16_t header = 0;
    uint16_t extendedHeader = 0; // For payloads > 125 bytes
    uint64_t superHeader = 0; // For payloads > 65,535 bytes

    // Only support single frames for now
    header |= (1 << 15); // FIN (1 bit)
    // RSV 1 to RSV 3 (1 bit each)
    header |= (type << 8); // OpCode (4 bits)
    header |= (0 << 7); // Mask (1 bit) (Off)
    if (payloadSize < 126)
    {
        header |= payloadSize;
        frame.insert(2, payload.constData(), payloadSize);
    }
    else if (payloadSize <= 65535)
    {
        header |= 126;
        extendedHeader = payloadSize;
        frame.insert(4, payload.constData(), payloadSize);
    }
    else
    {
        header |= 127;
        superHeader = payloadSize;
        frame.insert(10, payload.constData(), payloadSize);
    }

    frame[0] = (uint8_t)((header >> 8) & 0xFF);
    frame[1] = (uint8_t)(header & 0xFF);
    if (extendedHeader > 0)
    {
        frame[2] = (extendedHeader >> 8) & 0xFF;
        frame[3] = extendedHeader & 0xFF;
    }
    else if (superHeader > 0)
    {
        frame[2] = (superHeader >> 56) & 0xFF;
        frame[3] = (superHeader >> 48) & 0xFF;
        frame[4] = (superHeader >> 40) & 0xFF;
        frame[5] = (superHeader >> 32) & 0xFF;
        frame[6] = (superHeader >> 24) & 0xFF;
        frame[7] = (superHeader >> 16) & 0xFF;
        frame[8] = (superHeader >> 8) & 0xFF;
        frame[9] = (superHeader & 0xFF);
    }

    LOG(VB_HTTP, LOG_DEBUG, QString("WebSocketWorker::CreateFrame() - Frame size %1").arg(QString::number(frame.length())));

    return frame;
}

bool WebSocketWorker::SendFrame(const QByteArray &frame)
{
    if (!m_socket || !m_socket->isOpen() || !m_socket->isWritable())
    {
        SendClose(kCloseUnexpectedErr);
        return false;
    }

    LOG(VB_HTTP, LOG_DEBUG, QString("WebSocketWorker::SendFrame() - '%1'...").arg(QString(frame.left(64).toHex())));

    m_socket->write(frame.constData(), frame.length());

    return true;
}

bool WebSocketWorker::SendText(const QString &message)
{
    return SendText(message.trimmed().toUtf8());
}

bool WebSocketWorker::SendText(const QByteArray& message)
{
    if (!StringUtil::isValidUTF8(message))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("WebSocketWorker::SendText('%1...') - "
                                          "Text contains invalid UTF-8 character codes. Discarded.")
                                            .arg(QString(message.left(20))));
        return false;
    }

    QByteArray frame = CreateFrame(WebSocketFrame::kOpTextFrame, message);

    return !frame.isEmpty() && SendFrame(frame);
}

bool WebSocketWorker::SendBinary(const QByteArray& data)
{
    QByteArray frame = CreateFrame(WebSocketFrame::kOpBinaryFrame, data);

    return !frame.isEmpty() && SendFrame(frame);
}

bool WebSocketWorker::SendPing(const QByteArray& payload)
{
    QByteArray frame = CreateFrame(WebSocketFrame::kOpPing, payload);

    return !frame.isEmpty() && SendFrame(frame);
}

bool WebSocketWorker::SendPong(const QByteArray& payload)
{
    QByteArray frame = CreateFrame(WebSocketFrame::kOpPong, payload);

    return !frame.isEmpty() && SendFrame(frame);
}

bool WebSocketWorker::SendClose(ErrorCode errCode,
                                const QString &message)
{
    LOG(VB_HTTP, LOG_DEBUG, "WebSocketWorker::SendClose()");
    QByteArray payload;
    payload.resize(2);
    payload[0] = (uint8_t)(errCode >> 8);
    payload[1] = (uint8_t)(errCode & 0xFF);

    if (!message.isEmpty())
        payload.append(message.toUtf8());

    QByteArray frame = CreateFrame(WebSocketFrame::kOpClose, payload);

    if (!frame.isEmpty() && SendFrame(frame))
    {
        CloseConnection();
        return true;
    }

    CloseConnection();
    return false;
}

void WebSocketWorker::SendHeartBeat()
{
    SendPing(QByteArray("HeartBeat"));
}

void WebSocketWorker::RegisterExtension(WebSocketExtension* extension)
{
    if (!extension)
        return;

    connect(extension, &WebSocketExtension::SendTextMessage,
            this, qOverload<const QString &>(&WebSocketWorker::SendText));
    connect(extension, &WebSocketExtension::SendBinaryMessage,
            this, &WebSocketWorker::SendBinary);

    m_extensions.append(extension);
}

void WebSocketWorker::DeregisterExtension(WebSocketExtension* extension)
{
    if (!extension)
        return;

    for (auto it = m_extensions.begin(); it != m_extensions.end(); ++it)
    {
        if ((*it) == extension)
        {
            it = m_extensions.erase(it);
            break;
        }
    }
}


