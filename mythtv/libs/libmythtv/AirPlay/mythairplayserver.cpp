// TODO
// locking ?
// race on startup?
// http date format and locale

#include <QTcpSocket>
#include <QNetworkInterface>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QCryptographicHash>

#include "mthread.h"
#include "mythdate.h"
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythuiactions.h"
#include "mythmainwindow.h"
#include "mythuistatetracker.h"
#include "plist.h"
#include "tv_play.h"
#include "mythuinotificationcenter.h"

#include "bonjourregister.h"
#include "mythairplayserver.h"

MythAirplayServer* MythAirplayServer::gMythAirplayServer = NULL;
MThread*           MythAirplayServer::gMythAirplayServerThread = NULL;
QMutex*            MythAirplayServer::gMythAirplayServerMutex = new QMutex(QMutex::Recursive);

#define LOC QString("AirPlay: ")

#define HTTP_STATUS_OK                  200
#define HTTP_STATUS_SWITCHING_PROTOCOLS 101
#define HTTP_STATUS_NOT_IMPLEMENTED     501
#define HTTP_STATUS_UNAUTHORIZED        401

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
    if (size == 12 && id.toUpper() == id)
        return id;
    if (size != 12)
    {
        QByteArray ba;
        for (int i = 0; i < AIRPLAY_HARDWARE_ID_SIZE; i++)
            ba.append((random() % 80) + 33);
        id = ba.toHex();
    }
    id = id.toUpper();

    gCoreContext->SaveSetting(key, id);
    return id;
}

QString GenerateNonce(void)
{
    int nonceParts[4];
    QString nonce;
    QTime time = QTime::currentTime();
    qsrand((uint)time.msec());
    nonceParts[0] = qrand();
    nonceParts[1] = qrand();
    nonceParts[2] = qrand();
    nonceParts[3] = qrand();

    nonce =  QString::number(nonceParts[0], 16).toUpper();
    nonce += QString::number(nonceParts[1], 16).toUpper();
    nonce += QString::number(nonceParts[2], 16).toUpper();
    nonce += QString::number(nonceParts[3], 16).toUpper();
    return nonce;
}

QByteArray DigestMd5Response(QString response, QString option,
                             QString nonce, QString password,
                             QByteArray &auth)
{
    int authStart       = response.indexOf("response=\"") + 10;
    int authLength      = response.indexOf("\"", authStart) - authStart;
    auth                = response.mid(authStart, authLength).toLatin1();

    int uriStart        = response.indexOf("uri=\"") + 5;
    int uriLength       = response.indexOf("\"", uriStart) - uriStart;
    QByteArray uri      = response.mid(uriStart, uriLength).toLatin1();

    int userStart       = response.indexOf("username=\"") + 10;
    int userLength      = response.indexOf("\"", userStart) - userStart;
    QByteArray user     = response.mid(userStart, userLength).toLatin1();

    int realmStart      = response.indexOf("realm=\"") + 7;
    int realmLength     = response.indexOf("\"", realmStart) - realmStart;
    QByteArray realm    = response.mid(realmStart, realmLength).toLatin1();

    QByteArray passwd   = password.toLatin1();

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(user);
    hash.addData(":", 1);
    hash.addData(realm);
    hash.addData(":", 1);
    hash.addData(passwd);
    QByteArray ha1 = hash.result();
    ha1 = ha1.toHex();

    // calculate H(A2)
    hash.reset();
    hash.addData(option.toLatin1());
    hash.addData(":", 1);
    hash.addData(uri);
    QByteArray ha2 = hash.result().toHex();

    // calculate response
    hash.reset();
    hash.addData(ha1);
    hash.addData(":", 1);
    hash.addData(nonce.toLatin1());
    hash.addData(":", 1);
    hash.addData(ha2);
    return hash.result().toHex();
}

class APHTTPRequest
{
  public:
    APHTTPRequest(QByteArray& data) : m_readPos(0), m_data(data), m_size(0),
                                      m_incomingPartial(false)
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

    void Append(QByteArray& data)
    {
        m_body.append(data);
        Check();
    }

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

    bool IsComplete(void)
    {
        return !m_incomingPartial;
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
            m_size = m_headers["Content-Length"].toInt();
            if (m_size > 0 && remaining > 0)
            {
                m_body = m_data.mid(m_readPos, m_size);
                m_readPos += m_body.size();
            }
        }
    }

    void Check(void)
    {
        if (!m_incomingPartial)
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("HTTP Request:\n%1").arg(m_data.data()));
        }
        if (m_body.size() < m_size)
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                QString("AP HTTPRequest: Didn't read entire buffer."
                        "Left to receive: %1 (got %2 of %3) body=%4")
                .arg(m_size-m_body.size()).arg(m_readPos).arg(m_size).arg(m_body.size()));
            m_incomingPartial = true;
            return;
        }
        m_incomingPartial = false;
    }

    int        m_readPos;
    QByteArray m_data;
    QByteArray m_method;
    QByteArray m_uri;
    QList<QPair<QByteArray, QByteArray> > m_queries;
    QMap<QByteArray,QByteArray> m_headers;
    QByteArray m_body;
    int        m_size;
    bool       m_incomingPartial;
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
        QObject::connect(
            gMythAirplayServerThread->qthread(), SIGNAL(finished()),
            gMythAirplayServer,                  SLOT(Stop()));
        gMythAirplayServerThread->start(QThread::LowestPriority);
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Created airplay objects.");
    return true;
}

void MythAirplayServer::Cleanup(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Cleaning up.");

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
    m_lock(new QMutex(QMutex::Recursive)), m_setupPort(5100), m_id(-1)
{
}

MythAirplayServer::~MythAirplayServer()
{
    delete m_lock;
    m_lock = NULL;
    if (m_id > 0)
    {
        MythUINotificationCenter::GetInstance()->UnRegister(this, m_id);
        m_id = -1;
    }
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

    // remove all incoming buffers
    foreach (APHTTPRequest* request, m_incoming)
    {
        delete request;
    }
    m_incoming.clear();

    if (m_id > 0)
    {
        MythUINotificationCenter::GetInstance()->UnRegister(this, m_id);
        m_id = -1;
    }
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
        txt.append(14); txt.append("features=0x23b");
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

void MythAirplayServer::Stop(void)
{
    Teardown();
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

    deleteConnection(socket);
}

void MythAirplayServer::deleteConnection(QTcpSocket *socket)
{
    // must have lock
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
            !it.value().controlSocket)
        {
            if (!it.value().stopped)
            {
                StopSession(it.key());
            }
            remove = it.key();
            break;
        }
    }

    if (!remove.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Removing session '%1'")
            .arg(remove.data()));
        m_connections.remove(remove);

        // close any photos that could be displayed
        MythUINotificationCenter::GetInstance()->UnRegister(this, m_id);
        m_id = -1;

        MythNotification n(tr("Client disconnected"), tr("AirPlay"),
                           tr("from %1").arg(socket->peerAddress().toString()));
        MythUINotificationCenter::GetInstance()->Queue(n);
    }

    socket->deleteLater();

    if (m_incoming.contains(socket))
    {
        delete m_incoming[socket];
        m_incoming.remove(socket);
    }
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

    if (!m_incoming.contains(socket))
    {
        APHTTPRequest *request = new APHTTPRequest(buf);
        m_incoming.insert(socket, request);
    }
    else
    {
        m_incoming[socket]->Append(buf);
    }
    if (!m_incoming[socket]->IsComplete())
        return;
    HandleResponse(m_incoming[socket], socket);
    if (m_incoming.contains(socket))
    {
        delete m_incoming[socket];
        m_incoming.remove(socket);
    }
}

QByteArray MythAirplayServer::StatusToString(int status)
{
    switch (status)
    {
        case HTTP_STATUS_OK:                    return "OK";
        case HTTP_STATUS_SWITCHING_PROTOCOLS:   return "Switching Protocols";
        case HTTP_STATUS_NOT_IMPLEMENTED:       return "Not Implemented";
        case HTTP_STATUS_UNAUTHORIZED:          return "Unauthorized";
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
        session = addr.toString().toLatin1();
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

    if (m_connections[session].controlSocket != NULL &&
        m_connections[session].reverseSocket != NULL &&
        !m_connections[session].initialized)
    {
        // Got a full connection, disconnect any other clients
        DisconnectAllClients(session);
        m_connections[session].initialized = true;

        MythNotification n(tr("New Connection"), tr("AirPlay"),
                           tr("from %1").arg(socket->peerAddress().toString()));
        MythUINotificationCenter::GetInstance()->Queue(n);

        m_id = MythUINotificationCenter::GetInstance()->Register(this);

    }

    double position    = 0.0f;
    double duration    = 0.0f;
    float  playerspeed = 0.0f;
    bool   playing     = false;
    QString pathname;
    GetPlayerStatus(playing, playerspeed, position, duration, pathname);

    if (playing && pathname != m_pathname)
    {
        // not ours
        playing = false;
    }
    if (playing && duration > 0.01f && position < 0.01f)
    {
        // Assume playback hasn't started yet, get saved position
        position = m_connections[session].position;
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

    if (gCoreContext->GetNumSetting("AirPlayPasswordEnabled", false))
    {
        if (m_nonce.isEmpty())
        {
            m_nonce = GenerateNonce();
        }
        header = QString("WWW-Authenticate: Digest realm=\"AirPlay\", "
                         "nonce=\"%1\"\r\n").arg(m_nonce).toLatin1();
        if (!req->GetHeaders().contains("Authorization"))
        {
            SendResponse(socket, HTTP_STATUS_UNAUTHORIZED,
                         header, content_type, body);
            return;
        }

        QByteArray auth;
        if (DigestMd5Response(req->GetHeaders()["Authorization"], req->GetMethod(), m_nonce,
                              gCoreContext->GetSetting("AirPlayPassword"),
                              auth) == auth)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "AirPlay client authenticated");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "AirPlay authentication failed");
            SendResponse(socket, HTTP_STATUS_UNAUTHORIZED,
                         header, content_type, body);
            return;
        }
        header = "";
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
            SeekPosition(intpos);
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
        StopSession(session);
    }
    else if (req->GetURI() == "/photo")
    {
        if (req->GetMethod() == "PUT")
        {
            // this may be received before playback starts...
            QImage image = QImage::fromData(req->GetBody());
            bool png =
                req->GetBody().size() > 3 && req->GetBody()[1] == 'P' &&
                req->GetBody()[2] == 'N' && req->GetBody()[3] == 'G';
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Received %1 photo").arg(png ? "jpeg" : "png"));

            // send full screen display notification
            MythImageNotification n(MythNotification::New, image);
            n.SetId(m_id);
            n.SetParent(this);
            n.SetFullScreen(true);
            MythUINotificationCenter::GetInstance()->Queue(n);
        }
    }
    else if (req->GetURI() == "/slideshow-features")
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
            if (playerspeed > 0.0f)
            {
                PausePlayback();
            }
            SendReverseEvent(session, AP_EVENT_PAUSED);
        }
        else
        {
            if (playerspeed < 1.0f)
            {
                UnpausePlayback();
            }
            SendReverseEvent(session, AP_EVENT_PLAYING);
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

        if (!file.isEmpty())
        {
            m_pathname = file;
            StartPlayback(file);
            GetPlayerStatus(playing, playerspeed, position, duration, pathname);
            m_connections[session].url = QUrl(file);
            m_connections[session].position = start_pos * duration;
            SeekPosition(duration * start_pos);
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
    if (!socket || !m_incoming.contains(socket) ||
        socket->state() != QAbstractSocket::ConnectedState)
        return;
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
    }
    else
    {
        reply.append("Content-Length: 0");
    }
    reply.append("\r\n\r\n");

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
        case AP_EVENT_NONE:    return "none";
        default:               return "";
    }
}

void MythAirplayServer::GetPlayerStatus(bool &playing, float &speed,
                                        double &position, double &duration,
                                        QString &pathname)
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
    if (state.contains("pathname"))
        pathname = state["pathname"].toString();
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

void MythAirplayServer::StopSession(const QByteArray &session)
{
    m_connections[session].stopped = true;
    double position    = 0.0f;
    double duration    = 0.0f;
    float  playerspeed = 0.0f;
    bool   playing     = false;
    QString pathname;
    GetPlayerStatus(playing, playerspeed, position, duration, pathname);
    if (pathname != m_pathname)
    {
        // not ours
        return;
    }
    StopPlayback();
}

void MythAirplayServer::DisconnectAllClients(const QByteArray &session)
{
    QMutexLocker locker(m_lock);
    QHash<QByteArray,AirplayConnection>::iterator it = m_connections.begin();

    while (it != m_connections.end())
    {
        QTcpSocket *socket;

        if (it.key() == session)
        {
            ++it;
            continue;
        }
        if (!(*it).stopped)
        {
            StopSession(it.key());
        }
        socket = it.value().reverseSocket;
        if (socket)
        {
            socket->disconnect();
            socket->close();
            m_sockets.removeOne(socket);
            socket->deleteLater();
            if (m_incoming.contains(socket))
            {
                delete m_incoming[socket];
                m_incoming.remove(socket);
            }
        }
        socket = it.value().controlSocket;
        if (socket)
        {
            socket->disconnect();
            socket->close();
            m_sockets.removeOne(socket);
            socket->deleteLater();
            if (m_incoming.contains(socket))
            {
                delete m_incoming[socket];
                m_incoming.remove(socket);
            }
        }
        it = m_connections.erase(it);
    }
}

void MythAirplayServer::StartPlayback(const QString &pathname)
{
    if (TV::IsTVRunning())
    {
        StopPlayback();
    }
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("Sending ACTION_HANDLEMEDIA for %1")
        .arg(pathname));
    MythEvent* me = new MythEvent(ACTION_HANDLEMEDIA,
                                  QStringList(pathname));
    qApp->postEvent(GetMythMainWindow(), me);
    // Wait until we receive that the play has started
    gCoreContext->WaitUntilSignals(SIGNAL(TVPlaybackStarted()),
                                   SIGNAL(TVPlaybackAborted()),
                                   NULL);
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("ACTION_HANDLEMEDIA completed"));
}

void MythAirplayServer::StopPlayback(void)
{
    if (TV::IsTVRunning())
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Sending ACTION_STOP for %1")
            .arg(m_pathname));

        QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0,
                                      Qt::NoModifier, ACTION_STOP);
        qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
        // Wait until we receive that playback has stopped
        gCoreContext->WaitUntilSignals(SIGNAL(TVPlaybackStopped()),
                                       SIGNAL(TVPlaybackAborted()),
                                       NULL);
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("ACTION_STOP completed"));
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Playback not running, nothing to stop"));
    }
}

void MythAirplayServer::SeekPosition(uint64_t position)
{
    if (TV::IsTVRunning())
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Sending ACTION_SEEKABSOLUTE(%1) for %2")
            .arg(position)
            .arg(m_pathname));

        MythEvent* me = new MythEvent(ACTION_SEEKABSOLUTE,
                                      QStringList(QString::number((uint64_t)position)));
        qApp->postEvent(GetMythMainWindow(), me);
        // Wait until we receive that the seek has completed
        gCoreContext->WaitUntilSignals(SIGNAL(TVPlaybackSought(qint64)),
                                       SIGNAL(TVPlaybackStopped()),
                                       SIGNAL(TVPlaybackAborted()),
                                       NULL);
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("ACTION_SEEKABSOLUTE completed"));
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            QString("Trying to seek when playback hasn't started"));
    }
}

void MythAirplayServer::PausePlayback(void)
{
    if (TV::IsTVRunning() && !TV::CurrentTVInstance()->IsPaused())
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Sending ACTION_PAUSE for %1")
            .arg(m_pathname));

        QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0,
                                      Qt::NoModifier, ACTION_PAUSE);
        qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
        // Wait until we receive that playback has stopped
        gCoreContext->WaitUntilSignals(SIGNAL(TVPlaybackPaused()),
                                       SIGNAL(TVPlaybackStopped()),
                                       SIGNAL(TVPlaybackAborted()),
                                       NULL);
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("ACTION_PAUSE completed"));
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Playback not running, nothing to pause"));
    }
}

void MythAirplayServer::UnpausePlayback(void)
{
    if (TV::IsTVRunning())
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Sending ACTION_PLAY for %1")
            .arg(m_pathname));

        QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0,
                                      Qt::NoModifier, ACTION_PLAY);
        qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
        // Wait until we receive that playback has stopped
        gCoreContext->WaitUntilSignals(SIGNAL(TVPlaybackPlaying()),
                                       SIGNAL(TVPlaybackStopped()),
                                       SIGNAL(TVPlaybackAborted()),
                                       NULL);
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("ACTION_PLAY completed"));
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("Playback not running, nothing to unpause"));
    }
}
