// TODO
// locking ?
// race on startup?
// http date format and locale

#include <QTcpSocket>
#include <QNetworkInterface>
#include <QCoreApplication>
#include <QKeyEvent>

#include "mthread.h"
#include "mythdate.h"
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythuiactions.h"
#include "mythmainwindow.h"
#include "mythuistatetracker.h"
#include "plist.h"
#include "tv_play.h"

#include "bonjourregister.h"
#include "mythairplayserver.h"

MythAirplayServer* MythAirplayServer::gMythAirplayServer = NULL;
MThread*           MythAirplayServer::gMythAirplayServerThread = NULL;
QMutex*            MythAirplayServer::gMythAirplayServerMutex = new QMutex(QMutex::Recursive);

#define LOC QString("AirPlay: ")

#define HTTP_STATUS_OK                  200
#define HTTP_STATUS_SWITCHING_PROTOCOLS 101
#define HTTP_STATUS_NOT_IMPLEMENTED     501

#define AIRPLAY_SERVER_VERSION_STR ""
#define SERVER_INFO  QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>deviceid</key>\r\n"\
"<string>%1</string>\r\n"\
"<key>features</key>\r\n"\
"<integer>119</integer>\r\n"\
"<key>model</key>\r\n"\
"<string>AppleTV2,1</string>\r\n"\
"<key>protovers</key>\r\n"\
"<string>1.0</string>\r\n"\
"<key>srcvers</key>\r\n"\
"<string>101.28</string>\r\n"\
"</dict>\r\n"\
"</plist>\r\n")

#define EVENT_INFO QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>category</key>\r\n"\
"<string>video</string>\r\n"\
"<key>state</key>\r\n"\
"<string>%1</string>\r\n"\
"</dict>\r\n"\
"</plist>\r\n")

#define PLAYBACK_INFO  QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>duration</key>\r\n"\
"<real>%1</real>\r\n"\
"<key>loadedTimeRanges</key>\r\n"\
"<array>\r\n"\
"\t\t<dict>\r\n"\
"\t\t\t<key>duration</key>\r\n"\
"\t\t\t<real>%2</real>\r\n"\
"\t\t\t<key>start</key>\r\n"\
"\t\t\t<real>0.0</real>\r\n"\
"\t\t</dict>\r\n"\
"</array>\r\n"\
"<key>playbackBufferEmpty</key>\r\n"\
"<true/>\r\n"\
"<key>playbackBufferFull</key>\r\n"\
"<false/>\r\n"\
"<key>playbackLikelyToKeepUp</key>\r\n"\
"<true/>\r\n"\
"<key>position</key>\r\n"\
"<real>%3</real>\r\n"\
"<key>rate</key>\r\n"\
"<real>%4</real>\r\n"\
"<key>readyToPlay</key>\r\n"\
"<true/>\r\n"\
"<key>seekableTimeRanges</key>\r\n"\
"<array>\r\n"\
"\t\t<dict>\r\n"\
"\t\t\t<key>duration</key>\r\n"\
"\t\t\t<real>%1</real>\r\n"\
"\t\t\t<key>start</key>\r\n"\
"\t\t\t<real>0.0</real>\r\n"\
"\t\t</dict>\r\n"\
"</array>\r\n"\
"</dict>\r\n"\
"</plist>\r\n")

#define NOT_READY  QString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>readyToPlay</key>\r\n"\
"<false/>\r\n"\
"</dict>\r\n"\
"</plist>\r\n")

QString AirPlayHardwareId()
{
    QString key = "AirPlayId";
    QString id = gCoreContext->GetSetting(key);
    int size = id.size();
    if (size == 12)
        return id;

    QByteArray ba;
    for (int i = 0; i < AIRPLAY_HARDWARE_ID_SIZE; i++)
        ba.append((random() % 80) + 33);
    id = ba.toHex();

    gCoreContext->SaveSetting(key, id);
    return id;
}

class APHTTPRequest
{
  public:
    APHTTPRequest(QByteArray &data) : m_readPos(0), m_data(data)
    {
        Process();
        Check();
    }
   ~APHTTPRequest() { }

    QByteArray&       GetMethod(void)  { return m_method;   }
    QByteArray&       GetURI(void)     { return m_uri;      }
    QByteArray&       GetBody(void)    { return m_body;     }
    QMap<QByteArray,QByteArray>& GetHeaders(void)
                                       { return m_headers;  }

    QByteArray GetQueryValue(QByteArray key)
    {
        for (int i = 0; i < m_queries.size(); i++)
            if (m_queries[i].first == key)
                return m_queries[i].second;
        return "";
    }

    QMap<QByteArray,QByteArray> GetHeadersFromBody(void)
    {
        QMap<QByteArray,QByteArray> result;
        QList<QByteArray> lines = m_body.split('\n');;
        foreach (QByteArray line, lines)
        {
            int index = line.indexOf(":");
            if (index > 0)
            {
                result.insert(line.left(index).trimmed(),
                              line.mid(index + 1).trimmed());
            }
        }
        return result;
    }

  private:
    QByteArray GetLine(void)
    {
        int next = m_data.indexOf("\r\n", m_readPos);
        if (next < 0) return QByteArray();
        QByteArray line = m_data.mid(m_readPos, next - m_readPos);
        m_readPos = next + 2;
        return line;
    }

    void Process(void)
    {
        if (!m_data.size())
            return;

        // request line
        QByteArray line = GetLine();
        if (line.isEmpty())
            return;
        QList<QByteArray> vals = line.split(' ');
        if (vals.size() < 3)
            return;
        m_method = vals[0].trimmed();
        QUrl url = QUrl::fromEncoded(vals[1].trimmed());
        m_uri = url.encodedPath();
        m_queries = url.encodedQueryItems();
        if (m_method.isEmpty() || m_uri.isEmpty())
            return;

        // headers
        while (!(line = GetLine()).isEmpty())
        {
            int index = line.indexOf(":");
            if (index > 0)
            {
                m_headers.insert(line.left(index).trimmed(),
                                 line.mid(index + 1).trimmed());
            }
        }

        // body?
        if (m_headers.contains("Content-Length"))
        {
            int remaining = m_data.size() - m_readPos;
            int size = m_headers["Content-Length"].toInt();
            if (size > 0 && remaining > 0 && size <= remaining)
            {
                m_body = m_data.mid(m_readPos, size);
                m_readPos += size;
            }
        }
    }

    void Check(void)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("HTTP Request:\n%1").arg(m_data.data()));
        if (m_readPos == m_data.size())
            return;
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "AP HTTPRequest: Didn't read entire buffer.");
    }

    int  m_readPos;
    QByteArray m_data;
    QByteArray m_method;
    QByteArray m_uri;
    QList<QPair<QByteArray, QByteArray> > m_queries;
    QMap<QByteArray,QByteArray> m_headers;
    QByteArray m_body;
};

bool MythAirplayServer::Create(void)
{
    QMutexLocker locker(gMythAirplayServerMutex);

    // create the server thread
    if (!gMythAirplayServerThread)
        gMythAirplayServerThread = new MThread("AirplayServer");
    if (!gMythAirplayServerThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create airplay thread.");
        return false;
    }

    // create the server object
    if (!gMythAirplayServer)
        gMythAirplayServer = new MythAirplayServer();
    if (!gMythAirplayServer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create airplay object.");
        return false;
    }

    // start the thread
    if (!gMythAirplayServerThread->isRunning())
    {
        gMythAirplayServer->moveToThread(gMythAirplayServerThread->qthread());
        QObject::connect(
            gMythAirplayServerThread->qthread(), SIGNAL(started()),
            gMythAirplayServer,                  SLOT(Start()));
        gMythAirplayServerThread->start(QThread::LowestPriority);
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Created airplay objects.");
    return true;
}

void MythAirplayServer::Cleanup(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Cleaning up.");

    if (gMythAirplayServer)
        gMythAirplayServer->Teardown();

    QMutexLocker locker(gMythAirplayServerMutex);
    if (gMythAirplayServerThread)
    {
        gMythAirplayServerThread->exit();
        gMythAirplayServerThread->wait();
    }
    delete gMythAirplayServerThread;
    gMythAirplayServerThread = NULL;

    delete gMythAirplayServer;
    gMythAirplayServer = NULL;
}


MythAirplayServer::MythAirplayServer()
  : ServerPool(), m_name(QString("MythTV")), m_bonjour(NULL), m_valid(false),
    m_lock(new QMutex(QMutex::Recursive)), m_setupPort(5100)
{
}

MythAirplayServer::~MythAirplayServer()
{
    Teardown();

    delete m_lock;
    m_lock = NULL;
}

void MythAirplayServer::Teardown(void)
{
    QMutexLocker locker(m_lock);

    // invalidate
    m_valid = false;

    // disconnect from mDNS
    delete m_bonjour;
    m_bonjour = NULL;

    // disconnect connections
    foreach (QTcpSocket* connection, m_sockets)
    {
        disconnect(connection, 0, 0, 0);
        delete connection;
    }
    m_sockets.clear();
}

void MythAirplayServer::Start(void)
{
    QMutexLocker locker(m_lock);

    // already started?
    if (m_valid)
        return;

    // join the dots
    connect(this, SIGNAL(newConnection(QTcpSocket*)),
            this, SLOT(newConnection(QTcpSocket*)));

    // start listening for connections
    // try a few ports in case the default is in use
    int baseport = m_setupPort;
    m_setupPort = tryListeningPort(m_setupPort, AIRPLAY_PORT_RANGE);
    if (m_setupPort < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to find a port for incoming connections.");
    }
    else
    {
        // announce service
        m_bonjour = new BonjourRegister(this);
        if (!m_bonjour)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Bonjour object.");
            return;
        }

        // give each frontend a unique name
        int multiple = m_setupPort - baseport;
        if (multiple > 0)
            m_name += QString::number(multiple);

        QByteArray name = m_name.toUtf8();
        name.append(" on ");
        name.append(gCoreContext->GetHostName());
        QByteArray type = "_airplay._tcp";
        QByteArray txt;
        txt.append(26); txt.append("deviceid="); txt.append(GetMacAddress());
        txt.append(14); txt.append("features=0x219");
        txt.append(16); txt.append("model=AppleTV2,1");
        txt.append(14); txt.append("srcvers=101.28");

        if (!m_bonjour->Register(m_setupPort, type, name, txt))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to register service.");
            return;
        }
    }
    m_valid = true;
    return;
}

void MythAirplayServer::newConnection(QTcpSocket *client)
{
    QMutexLocker locker(m_lock);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("New connection from %1:%2")
        .arg(client->peerAddress().toString()).arg(client->peerPort()));

    m_sockets.append(client);
    connect(client, SIGNAL(disconnected()), this, SLOT(deleteConnection()));
    connect(client, SIGNAL(readyRead()), this, SLOT(read()));
}

void MythAirplayServer::deleteConnection(void)
{
    QMutexLocker locker(m_lock);
    QTcpSocket *socket = (QTcpSocket *)sender();
    if (!socket)
        return;

    if (!m_sockets.contains(socket))
        return;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Removing connection %1:%2")
        .arg(socket->peerAddress().toString()).arg(socket->peerPort()));
    m_sockets.removeOne(socket);

    QByteArray remove;
    QMutableHashIterator<QByteArray,AirplayConnection> it(m_connections);
    while (it.hasNext())
    {
        it.next();
        if (it.value().reverseSocket == socket)
            it.value().reverseSocket = NULL;
        if (it.value().controlSocket == socket)
            it.value().controlSocket = NULL;
        if (!it.value().reverseSocket &&
            !it.value().controlSocket &&
            it.value().stopped)
        {
            remove = it.key();
        }
    }

    if (!remove.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Removing session '%1'")
            .arg(remove.data()));
        m_connections.remove(remove);
    }

    socket->deleteLater();
}

void MythAirplayServer::read(void)
{
    QMutexLocker locker(m_lock);
    QTcpSocket *socket = (QTcpSocket *)sender();
    if (!socket)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Read for %1:%2")
        .arg(socket->peerAddress().toString()).arg(socket->peerPort()));

    QByteArray buf = socket->readAll();
    APHTTPRequest request(buf);
    HandleResponse(&request, socket);
}

QByteArray MythAirplayServer::StatusToString(int status)
{
    switch (status)
    {
        case HTTP_STATUS_OK: return "OK";
        case HTTP_STATUS_SWITCHING_PROTOCOLS: return "Switching Protocols";
        case HTTP_STATUS_NOT_IMPLEMENTED: return "Not Implemented";
    }
    return "";
}

void MythAirplayServer::HandleResponse(APHTTPRequest *req,
                                       QTcpSocket *socket)
{
    if (!socket)
        return;
    QHostAddress addr = socket->peerAddress();
    QByteArray session;
    QByteArray header;
    QString    body;
    int status = HTTP_STATUS_OK;
    QByteArray content_type;

    if (req->GetURI() != "/playback-info")
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Method: %1 URI: %2")
            .arg(req->GetMethod().data()).arg(req->GetURI().data()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Method: %1 URI: %2")
            .arg(req->GetMethod().data()).arg(req->GetURI().data()));
    }

    if (req->GetURI() == "200" || req->GetMethod().startsWith("HTTP"))
        return;

    if (!req->GetHeaders().contains("X-Apple-Session-ID"))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("No session ID in http request. "
                    "Connection from iTunes? Using IP %1").arg(addr.toString()));
    }
    else
    {
        session = req->GetHeaders()["X-Apple-Session-ID"];
    }

    if (session.size() == 0)
    {
        // No session ID, use IP address instead
        session = addr.toString().toAscii();
    }
    if (!m_connections.contains(session))
    {
        AirplayConnection apcon;
        m_connections.insert(session, apcon);
    }

    if (req->GetURI() == "/reverse")
    {
        QTcpSocket *s = m_connections[session].reverseSocket;
        if (s != socket && s != NULL)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Already have a different reverse socket for this connection.");
            return;
        }
        m_connections[session].reverseSocket = socket;
        status = HTTP_STATUS_SWITCHING_PROTOCOLS;
        header = "Upgrade: PTTH/1.0\r\nConnection: Upgrade\r\n";
        SendResponse(socket, status, header, content_type, body);
        return;
    }

    QTcpSocket *s = m_connections[session].controlSocket;
    if (s != socket && s != NULL)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Already have a different control socket for this connection.");
        return;
    }
    m_connections[session].controlSocket = socket;

    double position    = 0.0f;
    double duration    = 0.0f;
    float  playerspeed = 0.0f;
    bool   playing     = false;
    GetPlayerStatus(playing, playerspeed, position, duration);
    // set initial position if it was set at start of playback.
    if (duration > 0.01f && playing)
    {
        if (m_connections[session].initial_position > 0.0f)
        {
            position = duration * m_connections[session].initial_position;
            MythEvent* me = new MythEvent(ACTION_SEEKABSOLUTE,
                                          QStringList(QString::number((uint64_t)position)));
            qApp->postEvent(GetMythMainWindow(), me);
            m_connections[session].position = position;
            m_connections[session].initial_position = -1.0f;
        }
        else if (position < .01f)
        {
            // Assume playback hasn't started yet, get saved position
            position = m_connections[session].position;
        }
    }
    if (!playing && m_connections[session].was_playing)
    {
        // playback got interrupted, notify client to stop
        if (SendReverseEvent(session, AP_EVENT_STOPPED))
        {
            m_connections[session].was_playing = false;
        }
    }
    else
    {
        m_connections[session].was_playing = playing;
    }

    if (req->GetURI() == "/server-info")
    {
        content_type = "text/x-apple-plist+xml\r\n";
        body = SERVER_INFO;
        body.replace("%1", GetMacAddress());
        LOG(VB_GENERAL, LOG_INFO, body);
    }
    else if (req->GetURI() == "/scrub")
    {
        double pos = req->GetQueryValue("position").toDouble();
        if (req->GetMethod() == "POST")
        {
            // this may be received before playback starts...
            uint64_t intpos = (uint64_t)pos;
            m_connections[session].position = pos;
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Scrub: (post) seek to %1").arg(intpos));
            MythEvent* me = new MythEvent(ACTION_SEEKABSOLUTE,
                                          QStringList(QString::number(intpos)));
            qApp->postEvent(GetMythMainWindow(), me);
        }
        else if (req->GetMethod() == "GET")
        {
            content_type = "text/parameters\r\n";
            body = QString("duration: %1\r\nposition: %2\r\n")
                .arg((double)duration, 0, 'f', 6, '0')
                .arg((double)position, 0, 'f', 6, '0');

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Scrub: (get) returned %1 of %2")
                .arg(position).arg(duration));

            /*
            if (playing && playerspeed < 1.0f)
            {
                SendReverseEvent(session, AP_EVENT_PLAYING);
                QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0,
                                              Qt::NoModifier, ACTION_PLAY);
                qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
            }
            */
        }
    }
    else if (req->GetURI() == "/stop")
    {
        m_connections[session].stopped = true;
        QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0,
                                      Qt::NoModifier, ACTION_STOP);
        qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
    }
    else if (req->GetURI() == "/photo" ||
             req->GetURI() == "/slideshow-features")
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "Slideshow functionality not implemented.");
    }
    else if (req->GetURI() == "/authorize")
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Ignoring authorize request.");
    }
    else if (req->GetURI() == "/rate")
    {
        float rate = req->GetQueryValue("value").toFloat();
        m_connections[session].speed = rate;

        if (rate < 1.0f)
        {
            SendReverseEvent(session, AP_EVENT_PAUSED);
            if (playerspeed > 0.0f)
            {
                QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0,
                                              Qt::NoModifier, ACTION_PAUSE);
                qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
            }
        }
        else
        {
            SendReverseEvent(session, AP_EVENT_PLAYING);
            if (playerspeed < 1.0f)
            {
                QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0,
                                              Qt::NoModifier, ACTION_PLAY);
                qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
            }
        }
    }
    else if (req->GetURI() == "/play")
    {
        QByteArray file;
        double start_pos = 0.0f;
        if (req->GetHeaders().contains("Content-Type") &&
            req->GetHeaders()["Content-Type"] == "application/x-apple-binary-plist")
        {
            PList plist(req->GetBody());
            LOG(VB_GENERAL, LOG_DEBUG, LOC + plist.ToString());

            QVariant start   = plist.GetValue("Start-Position");
            QVariant content = plist.GetValue("Content-Location");
            if (start.isValid() && start.canConvert<double>())
                start_pos = start.toDouble();
            if (content.isValid() && content.canConvert<QByteArray>())
                file = content.toByteArray();
        }
        else
        {
            QMap<QByteArray,QByteArray> headers = req->GetHeadersFromBody();
            file  = headers["Content-Location"];
            start_pos = headers["Start-Position"].toDouble();
        }

        if (!TV::IsTVRunning())
        {
            if (!file.isEmpty())
            {
                m_connections[session].url = QUrl(file);
                m_connections[session].position = 0.0f;
                m_connections[session].initial_position = start_pos;

                MythEvent* me = new MythEvent(ACTION_HANDLEMEDIA,
                                              QStringList(file));
                qApp->postEvent(GetMythMainWindow(), me);
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Ignoring playback - something else is playing.");
        }

        SendReverseEvent(session, AP_EVENT_PLAYING);
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("File: '%1' start_pos '%2'")
            .arg(file.data()).arg(start_pos));
    }
    else if (req->GetURI() == "/playback-info")
    {
        content_type = "text/x-apple-plist+xml\r\n";

        if (!playing)
        {
            body = NOT_READY;
            SendReverseEvent(session, AP_EVENT_LOADING);
        }
        else
        {
            body = PLAYBACK_INFO;
            body.replace("%1", QString("%1").arg((double)duration, 0, 'f', 6, '0'));
            body.replace("%2", QString("%1").arg((double)duration, 0, 'f', 6, '0')); // cached
            body.replace("%3", QString("%1").arg((double)position, 0, 'f', 6, '0'));
            body.replace("%4", playerspeed > 0.0f ? "1.0" : "0.0");
            LOG(VB_GENERAL, LOG_DEBUG, body);
            SendReverseEvent(session, playerspeed > 0.0f ? AP_EVENT_PLAYING :
                                                           AP_EVENT_PAUSED);
        }
    }
    SendResponse(socket, status, header, content_type, body);
}

void MythAirplayServer::SendResponse(QTcpSocket *socket,
                                     int status, QByteArray header,
                                     QByteArray content_type, QString body)
{
    QTextStream response(socket);
    response.setCodec("UTF-8");
    QByteArray reply;
    reply.append("HTTP/1.1 ");
    reply.append(QString::number(status));
    reply.append(" ");
    reply.append(StatusToString(status));
    reply.append("\r\n");
    reply.append("DATE: ");
    reply.append(MythDate::current().toString("ddd, d MMM yyyy hh:mm:ss"));
    reply.append(" GMT\r\n");
    if (header.size())
        reply.append(header);

    if (body.size())
    {
        reply.append("Content-Type: ");
        reply.append(content_type);
        reply.append("Content-Length: ");
        reply.append(QString::number(body.size()));
        reply.append("\r\n");
    }

    reply.append("\r\n");

    if (body.size())
        reply.append(body);

    response << reply;
    response.flush();

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Send: %1 \n\n%2\n")
        .arg(socket->flush()).arg(reply.data()));
}

bool MythAirplayServer::SendReverseEvent(QByteArray &session,
                                         AirplayEvent event)
{
    if (!m_connections.contains(session))
        return false;
    if (m_connections[session].lastEvent == event)
        return false;
    if (!m_connections[session].reverseSocket)
        return false;

    QString body;
    if (AP_EVENT_PLAYING == event ||
        AP_EVENT_LOADING == event ||
        AP_EVENT_PAUSED  == event ||
        AP_EVENT_STOPPED == event)
    {
        body = EVENT_INFO;
        body.replace("%1", eventToString(event));
    }

    m_connections[session].lastEvent = event;
    QTextStream response(m_connections[session].reverseSocket);
    response.setCodec("UTF-8");
    QByteArray reply;
    reply.append("POST /event HTTP/1.1\r\n");
    reply.append("Content-Type: text/x-apple-plist+xml\r\n");
    reply.append("Content-Length: ");
    reply.append(QString::number(body.size()));
    reply.append("\r\n");
    reply.append("x-apple-session-id: ");
    reply.append(session);
    reply.append("\r\n\r\n");
    if (body.size())
        reply.append(body);

    response << reply;
    response.flush();

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Send reverse: %1 \n\n%2\n")
         .arg(m_connections[session].reverseSocket->flush())
         .arg(reply.data()));
    return true;
}

QString MythAirplayServer::eventToString(AirplayEvent event)
{
    switch (event)
    {
        case AP_EVENT_PLAYING: return "playing";
        case AP_EVENT_PAUSED:  return "paused";
        case AP_EVENT_LOADING: return "loading";
        case AP_EVENT_STOPPED: return "stopped";
    }
    return "";
}

void MythAirplayServer::GetPlayerStatus(bool &playing, float &speed,
                                        double &position, double &duration)
{
    QVariantMap state;
    MythUIStateTracker::GetFreshState(state);
    if (state.contains("state"))
        playing = state["state"].toString() != "idle";
    if (state.contains("playspeed"))
        speed = state["playspeed"].toFloat();
    if (state.contains("secondsplayed"))
        position = state["secondsplayed"].toDouble();
    if (state.contains("totalseconds"))
        duration = state["totalseconds"].toDouble();
}

QString MythAirplayServer::GetMacAddress()
{
    QString id = AirPlayHardwareId();

    QString res;
    for (int i = 1; i <= id.size(); i++)
    {
        res.append(id[i-1]);
        if (i % 2 == 0 && i != id.size())
        {
            res.append(':');
        }
    }
    return res;
}
