// Qt
#include <QThread>
#include <QTcpSocket>
#ifndef QT_NO_OPENSSL
#include <QSslSocket>
#endif

// MythTV
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "http/mythwebsocket.h"
#include "http/mythhttps.h"
#include "http/mythhttpsocket.h"
#include "http/mythhttpdata.h"
#include "http/mythhttpfile.h"
#include "http/mythhttpresponse.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpranges.h"
#include "http/mythhttpservices.h"
#include "http/mythwebsocketevent.h"

// Std
#include <chrono>
using namespace std::chrono_literals;

#define LOC QString(m_peer + ": ")

MythHTTPSocket::MythHTTPSocket(qintptr Socket, bool SSL, MythHTTPConfig Config)
  : m_socketFD(Socket),
    m_config(std::move(Config))
{
    // Connect Finish signal to Stop
    connect(this, &MythHTTPSocket::Finish, this, &MythHTTPSocket::Stop);

    // Create socket
    QSslSocket* sslsocket = nullptr;
    if (SSL)
    {
#ifndef QT_NO_OPENSSL
        sslsocket = new QSslSocket(this);
        m_socket = sslsocket;
#endif
    }
    else
    {
        m_socket = new QTcpSocket(this);
    }

    if (!m_socket)
    {
        Stop();
        return;
    }

    connect(m_socket, &QTcpSocket::readyRead,    this, &MythHTTPSocket::Read);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &MythHTTPSocket::Write);
    connect(m_socket, &QTcpSocket::disconnected, this, &MythHTTPSocket::Disconnected);
    connect(m_socket, &QTcpSocket::disconnected, QThread::currentThread(), &QThread::quit);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &MythHTTPSocket::Error);
    m_socket->setSocketDescriptor(m_socketFD);

#ifndef QT_NO_OPENSSL
    if (SSL && sslsocket)
        MythHTTPS::InitSSLSocket(sslsocket, m_config.m_sslConfig);
#endif

    m_peer = QString("%1:%2").arg(m_socket->peerAddress().toString()).arg(m_socket->peerPort());
    LOG(VB_HTTP, LOG_INFO, LOC + "New connection");

    // Setup up timeout timer
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MythHTTPSocket::Timeout);
    m_timer.start(m_config.m_timeout);

    if (!gCoreContext->CheckSubnet(m_socket))
        Stop();

    // Setup root services handler - a service for services
    // Note: the services service is not self aware - i.e. it doesn't list itself
    // as a service. This is intentional; a client needs to know of the existence
    // of /services/ already - otherwise it wouldn't be calling it and the 'services'
    // service would appear again as /services/services/
    auto service = MythHTTPService::Create<MythHTTPServices>();
    m_activeServices.emplace_back(service);
    auto * services = dynamic_cast<MythHTTPServices*>(service.get());
    if (services == nullptr)
    {
        LOG(VB_HTTP, LOG_ERR, LOC + "Failed to get root services handler.");
        return;
    }
    connect(this, &MythHTTPSocket::UpdateServices, services, &MythHTTPServices::UpdateServices);
    services->UpdateServices(m_config.m_services);
}

MythHTTPSocket::~MythHTTPSocket()
{
    delete m_websocketevent;
    delete m_websocket;
    if (m_socket)
        m_socket->close();
    delete m_socket;
}

/*! \brief Update our list of recognised file paths.
 *
 * The initial set of paths is passed in via the constructor. Any changes are
 * notified using this slot.
*/
void MythHTTPSocket::PathsChanged(const QStringList& Paths)
{
    m_config.m_filePaths = Paths;
}

void MythHTTPSocket::HandlersChanged(const HTTPHandlers& Handlers)
{
    m_config.m_handlers = Handlers;
}

void MythHTTPSocket::ServicesChanged(const HTTPServices& Services)
{
    m_config.m_services = Services;
    emit UpdateServices(m_config.m_services);
}

/*! \brief Update the list of allowed Origins.
 *
 * The initial list is passed in via the constructor and any changes are notified
 * via this slot.
*/
void MythHTTPSocket::OriginsChanged(const QStringList& Origins)
{
    m_config.m_allowedOrigins = Origins;
}

void MythHTTPSocket::HostsChanged(const QStringList& Hosts)
{
    m_config.m_hosts = Hosts;
}

/*! \brief The socket was disconnected.
 *
 * The QTcpSocket::disconnected signal is also connected to our QThread::quit slot,
 * so the thread will be closed once the socket is disconnected.
*/
void MythHTTPSocket::Disconnected()
{
    LOG(VB_HTTP, LOG_INFO, LOC + "Disconnected");
}

void MythHTTPSocket::Error(QAbstractSocket::SocketError Error)
{
    if (Error != QAbstractSocket::RemoteHostClosedError)
        LOG(VB_HTTP, LOG_INFO, LOC + QString("Error %1 (%2)").arg(Error).arg(m_socket->errorString()));
}

/*! \brief Close the socket after a period of inactivity.
 *
 * This ensures we recycle unused threads for new connections.
*/
void MythHTTPSocket::Timeout()
{
    LOG(VB_HTTP, LOG_DEBUG, LOC + "Timed out waiting for activity");
    // Send '408 Request Timeout' - Respond will close the socket.
    // Note: some clients will close the socket themselves at the correct time -
    // in which case the disconnect may preceed the timeout here - so avoid
    // misleading write errors...
    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        // If this has timed out while a write is in progress, then the socket
        // buffers are not empty and we cannot send a response - so just close.
        if (m_socket->bytesToWrite() || !m_queue.empty())
        {
            m_queue.clear();
            LOG(VB_HTTP, LOG_INFO, LOC + "Timeout during write. Quitting.");
            Stop();
            return;
        }
        Respond(MythHTTPResponse::ErrorResponse(HTTPRequestTimeout, m_config.m_serverName));
        Stop();
    }
}

/*! \brief Close the socket and quit the thread.
 *
 * This is triggered by the activity timeout, invalid requests, if signalled
 * by the parent server (i.e. closing down) or when the connection is closed
 * after a request.
*/
void MythHTTPSocket::Stop()
{
    LOG(VB_HTTP, LOG_INFO, LOC + "Stop");
    if (m_websocket)
        m_websocket->Close();
    m_timer.stop();
    m_stopping = true;
    // Note: previously this called QTcpSocket::disconnectFromHost - but when
    // an interrupt is received (i.e. quit the app), there is an intermittent
    // race condition where the socket is disconnected before the thread is told
    // to quit and the call to QThread::quit is not triggered (i.e. the connection
    // between QTcpSocket::disconnected and QThread::quit does not work). So
    // just quit the thread.
    QThread::currentThread()->quit();
}

/*! \brief Read data from the socket which is parsed by MythHTTPParser
*
* If a complete request is received, move on to ProcessRequest.
*/
void MythHTTPSocket::Read()
{
    if (m_stopping)
        return;

    // Warn if we haven't sent the last response
    if (!m_queue.empty())
        LOG(VB_GENERAL, LOG_WARNING, LOC + "New request but queue is not empty");

    // reset the idle timer
    m_timer.start(m_config.m_timeout);

    bool ready = false;
    if (!m_parser.Read(m_socket, ready))
    {
        LOG(VB_HTTP, LOG_WARNING, LOC + "HTTP error");
        Stop();
        return;
    }

    if (!ready)
        return;

    // We have a completed request
    HTTPRequest2  request = m_parser.GetRequest(m_config, m_socket);
    HTTPResponse response = nullptr;

    // Request should have initial OK status if valid
    if (request->m_status != HTTPOK)
    {
        response = MythHTTPResponse::ErrorResponse(request);
        Respond(response);
        return;
    }

    // Websocket upgrade request...
    // This will have a connection type of HTTPConnectionUpgrade on success.
    // Once the response is sent, we will create a WebSocket instance to handle
    // further read/write signals.
    if (!MythHTTP::GetHeader(request->m_headers, "upgrade").isEmpty())
        response = MythHTTPResponse::UpgradeResponse(request, m_protocol, m_testSocket);

    // Try (possibly file specific) handlers
    if (response == nullptr)
    {
        // cppcheck-suppress unassignedVariable
        for (const auto& [path, function] : m_config.m_handlers)
        {
            if (path == request->m_url.toString())
            {
                response = std::invoke(function, request);
                if (response)
                    break;
            }
        }
    }

    const QString& rpath = request->m_path;

    // Try active services first - this is currently the services root only
    if (response == nullptr)
    {
        LOG(VB_HTTP, LOG_INFO, LOC + QString("Processing: path '%1' file '%2'")
            .arg(rpath, request->m_fileName));

        for (auto & service : m_activeServices)
        {
            if (service->Name() == rpath)
            {
                response = service->HTTPRequest(request);
                if (response)
                    break;
            }
        }
    }

    // Then 'inactive' services
    if (response == nullptr)
    {
        // cppcheck-suppress unassignedVariable
        for (const auto & [path, constructor] : m_config.m_services)
        {
            if (path == rpath)
            {
                auto instance = std::invoke(constructor);
                response = instance->HTTPRequest(request);
                if (response)
                    break;
                // the service object will be deleted here as it goes out of scope
                // but we could retain a cache...
            }
        }
    }

    // Try (dynamic) handlers
    if (response == nullptr)
    {
        // cppcheck-suppress unassignedVariable
        for (const auto& [path, function] : m_config.m_handlers)
        {
            if (path == rpath)
            {
                response = std::invoke(function, request);
                if (response)
                    break;
            }
        }
    }

    // then simple file path handlers
    if (response == nullptr)
    {
        for (const auto & path : std::as_const(m_config.m_filePaths))
        {
            if (path == rpath)
            {
                response = MythHTTPFile::ProcessFile(request);
                if (response)
                    break;
            }
        }
    }

    // Try error page handler
    if (response == nullptr || response->m_status == HTTPNotFound)
    {
        if(m_config.m_errorPageHandler.first.length() > 0)
        {
            auto function = m_config.m_errorPageHandler.second;
            response = std::invoke(function, request);
        }
    }

    // nothing to see
    if (response == nullptr)
    {
        request->m_status = HTTPNotFound;
        response = MythHTTPResponse::ErrorResponse(request);
    }

    // Send the response
    Respond(response);
}

/*! \brief Send response to client.
 *
 * Queue headers, queue content, signal whether to close the socket and push
 * first data to start the write.
*/
void MythHTTPSocket::Respond(const HTTPResponse& Response)
{
    if (!Response || (m_socket->state() != QAbstractSocket::ConnectedState))
        return;

    // Warn if the last response has not been completed
    if (!m_queue.empty())
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Responding but queue is not empty");

    // Reset the write tracker
    m_writeTime.start();
    m_totalToSend  = 0;
    m_totalWritten = 0;
    m_totalSent    = 0;
    m_writeBuffer  = nullptr;

    // Finalise the response
    Response->Finalise(m_config);

    // Queue headers
    for (const auto & header : std::as_const(Response->m_responseHeaders))
    {
        LOG(VB_HTTP, LOG_DEBUG, header->trimmed().constData());
        m_queue.emplace_back(header);
    }

    // Queue in memory response content or file
    if (!std::get_if<std::monostate>(&Response->m_response))
        m_queue.push_back(Response->m_response);
    else
        LOG(VB_HTTP, LOG_DEBUG, LOC + "No content in response");

    // Sum the expected number of bytes to be written. This is the size of the
    // data or file OR the total size of the range request. For multipart range
    // requests, add the total size of the additional headers.
    for (const auto & content : std::as_const(m_queue))
    {
        if (const auto * data = std::get_if<HTTPData>(&content))
        {
            m_totalToSend += (*data)->m_partialSize > 0 ? (*data)->m_partialSize : (*data)->size();
            m_totalToSend += (*data)->m_multipartHeaderSize;
        }
        else if (const auto * file = std::get_if<HTTPFile>(&content))
        {
            m_totalToSend += (*file)->m_partialSize > 0 ? (*file)->m_partialSize : (*file)->size();
            m_totalToSend += (*file)->m_multipartHeaderSize;
        }
    }

    // Signal what to do once the response is sent (close, keep alive or upgrade)
    m_nextConnection = Response->m_connection;

    // Get the ball rolling
    Write();
}

/*! \brief Send an (error) response directly without creating a thread.
 *
 * This is used to send 503 Service Unavailable
*/
void MythHTTPSocket::RespondDirect(qintptr Socket, const HTTPResponse& Response, const MythHTTPConfig &Config)
{
    if (!(Socket && Response))
        return;

    Response->Finalise(Config);
    auto * socket = new QTcpSocket();
    socket->setSocketDescriptor(Socket);
    for (const auto & header : std::as_const(Response->m_responseHeaders))
        socket->write(*header);
    socket->flush();
    socket->disconnectFromHost();
    delete socket;
}

void MythHTTPSocket::Write(int64_t Written)
{
    auto chunkheader = [&](const QByteArray& Data)
    {
        int64_t wrote = m_socket->write(Data);
        if (wrote < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "Error writing chunk header");
            return;
        }
        m_totalToSend += wrote;
        m_totalWritten += wrote;
    };

    if (m_stopping)
        return;

    // Naughty clients have a habit of stopping reading but leaving the socket open.
    // Instead of stopping the timer when a request has been received, we leave
    // it running and update when we see any read OR write activity
    // TODO perhaps worth using a larger timeout (e.g. *2) for writes - for clients
    // that are streaming video and have buffered a large amount of data - but with range
    // request support this probably shouldn't matter - as they can just reconnect
    // and pick up where they left off.
    m_timer.start(m_config.m_timeout);

    // Update write tracker
    m_totalSent += Written;
    int64_t buffered = m_socket->bytesToWrite();
    LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Socket sent %1bytes (still to send %2)")
        .arg(Written).arg(buffered));

    // If the client cannot consume data as fast as we are sending it, we just
    // buffer more and more data in the Qt socket code. So return and wait for
    // the next write.
    if (buffered > (HTTP_CHUNKSIZE << 1))
    {
        LOG(VB_HTTP, LOG_DEBUG, LOC + "Draining buffers");
        return;
    }

    if (m_totalSent >= m_totalToSend)
    {
        auto seconds = static_cast<double>(m_writeTime.nsecsElapsed()) / 1000000000.0;
        auto rate = static_cast<uint64_t>(static_cast<double>(m_totalSent) / seconds);
        LOG(VB_HTTP, LOG_INFO, LOC + QString("Wrote %1bytes in %2seconds (%3)")
            .arg(m_totalSent).arg(seconds, 8, 'f', 6, '0')
            .arg(MythHTTPWS::BitrateToString(rate)));

        if (m_queue.empty())
        {
            if (m_nextConnection == HTTPConnectionClose)
                Stop();
            else if (m_nextConnection == HTTPConnectionUpgrade)
                SetupWebSocket();
            return;
        }
        // This is going to be unrecoverable
        LOG(VB_GENERAL, LOG_ERR, LOC + "Write complete but queue not empty");
        Stop();
        return;
    }

    // Fill them buffers
    int64_t available = HTTP_CHUNKSIZE;
    while ((available > 0) && !m_queue.empty())
    {
        int64_t written  = 0;
        int64_t read     = 0;
        int64_t wrote    = 0;
        int64_t itemsize = 0;
        int64_t towrite  = 0;
        bool chunk = false;
        auto * data = std::get_if<HTTPData>(&m_queue.front());
        auto * file = std::get_if<HTTPFile>(&m_queue.front());

        if (data)
        {
            chunk    = (*data)->m_encoding == HTTPChunked;
            written  = (*data)->m_written;
            itemsize = (*data)->m_partialSize > 0 ? (*data)->m_partialSize : static_cast<int64_t>((*data)->size());
            towrite  = std::min(itemsize - written, available);
            int64_t offset = 0;
            HTTPMulti multipart { nullptr, nullptr };
            if (!(*data)->m_ranges.empty())
                multipart = MythHTTPRanges::HandleRangeWrite(*data, available, towrite, offset);
            if (chunk)
                chunkheader(QStringLiteral("%1\r\n").arg(towrite, 0, 16).toLatin1().constData());
            if (multipart.first)
                wrote += m_socket->write(multipart.first->constData());
            wrote += m_socket->write((*data)->constData() + written + offset, towrite);
            if (multipart.second)
                wrote += m_socket->write(multipart.second->constData());
            if (chunk)
                chunkheader("\r\n");
        }
        else if (file)
        {
            chunk    = (*file)->m_encoding == HTTPChunked;
            written  = (*file)->m_written;
            itemsize = (*file)->m_partialSize > 0 ? (*file)->m_partialSize : static_cast<int64_t>((*file)->size());
            towrite  = std::min(itemsize - written, available);
            HTTPMulti multipart { nullptr, nullptr };
            if (!(*file)->m_ranges.empty())
            {
                int64_t offset = 0;
                multipart = MythHTTPRanges::HandleRangeWrite(*file, available, towrite, offset);
                if (offset != (*file)->pos())
                    (*file)->seek(offset);
            }
            int64_t writebufsize = std::min(itemsize, static_cast<int64_t>(HTTP_CHUNKSIZE));
            if (m_writeBuffer && (m_writeBuffer->size() < writebufsize))
                m_writeBuffer = nullptr;
            if (!m_writeBuffer)
                m_writeBuffer = MythHTTPData::Create(static_cast<int>(writebufsize), '\0');
            read = (*file)->read(m_writeBuffer->data(), towrite);
            if (read > 0)
            {
                if (chunk)
                    chunkheader(QStringLiteral("%1\r\n").arg(read, 0, 16).toLatin1().constData());
                if (multipart.first)
                    wrote += m_socket->write(multipart.first->constData());
                wrote += m_socket->write(m_writeBuffer->data(), read);
                if (multipart.second)
                    wrote += m_socket->write(multipart.second->constData());
                if (chunk)
                    chunkheader("\r\n");
            }
        }

        if (wrote < 0 || read < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Write error (Read: %1 Write: %2)")
                .arg(read).arg(wrote));
            Stop();
            return;
        }

        written += wrote;
        if (data) (*data)->m_written = written;
        if (file) (*file)->m_written = written;
        m_totalWritten += wrote;
        available -= wrote;

#if 0
        LOG(VB_HTTP, LOG_DEBUG, LOC + QString("Sent %1 bytes (cum: %2 of %3)")
            .arg(wrote).arg(m_totalWritten).arg(m_totalToSend));
#endif

        // Move on to the next buffer?
        if (written >= itemsize)
        {
            if (chunk)
                chunkheader("0\r\n\r\n");
            m_queue.pop_front();
            m_writeBuffer = nullptr;
        }
    }
}

/*! \brief Transition socket to a WebSocket
*/
void MythHTTPSocket::SetupWebSocket()
{
    // Disconnect and stop our read/write timer
    m_timer.disconnect(this);
    m_timer.stop();

    // Disconnect our socket read/writes
    disconnect(m_socket, &QTcpSocket::readyRead, this, &MythHTTPSocket::Read);
    disconnect(m_socket, &QTcpSocket::bytesWritten, this, &MythHTTPSocket::Write);

    // Create server websocket instance
    m_websocket = MythWebSocket::CreateWebsocket(true, m_socket, m_protocol, m_testSocket);
    if (!m_websocket)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create websocket");
        Stop();
        return;
    }
    // Create event listener
    m_websocketevent = new MythWebSocketEvent();
    if (!m_websocketevent)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create websocket event listener");
        Stop();
        return;
    }

    // Listen for messages
    connect(m_websocket, &MythWebSocket::NewTextMessage,   this, &MythHTTPSocket::NewTextMessage);
    connect(m_websocket, &MythWebSocket::NewRawTextMessage,this, &MythHTTPSocket::NewRawTextMessage);
    connect(m_websocket, &MythWebSocket::NewBinaryMessage, this, &MythHTTPSocket::NewBinaryMessage);

    // Sending messages
    connect(m_websocketevent, &MythWebSocketEvent::SendTextMessage, m_websocket, &MythWebSocket::SendTextFrame);

    // Add this thread to the list of upgraded threads, so we free up slots
    // for regular HTTP sockets.
    emit ThreadUpgraded(QThread::currentThread());
}

void MythHTTPSocket::NewTextMessage(const StringPayload& Text)
{
    if (!Text)
        return;
    m_websocketevent->HandleTextMessage(Text);
}

void MythHTTPSocket::NewRawTextMessage(const DataPayloads& Payloads)
{
    if (Payloads.empty())
        return;
    m_websocketevent->HandleRawTextMessage(Payloads);
}

void MythHTTPSocket::NewBinaryMessage(const DataPayloads& Payloads)
{
    if (Payloads.empty())
        return;
}
