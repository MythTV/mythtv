// Qt
#include <QDirIterator>
#include <QNetworkInterface>
#include <QCoreApplication>
#include <QSslKey>
#include <QSslCipher>
#include <QSslCertificate>

// MythTV
#include "mythversion.h"
#include "mythdirs.h"
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "libmythbase/configuration.h"
#if CONFIG_LIBDNS_SD
#include "bonjourregister.h"
#endif
#include "http/mythhttpsocket.h"
#include "http/mythhttpresponse.h"
#include "http/mythhttpthread.h"
#include "http/mythhttps.h"
#include "http/mythhttpserver.h"

// Std
#ifndef _WIN32
#include <sys/utsname.h>
#endif

#define LOC QString("HTTPServer: ")

MythHTTPServer::MythHTTPServer()
{
    // Join the dots
    connect(this, &MythHTTPServer::EnableHTTP,     this, &MythHTTPServer::EnableDisable);
    connect(this, &MythHTTPServer::AddPaths,       this, &MythHTTPServer::NewPaths);
    connect(this, &MythHTTPServer::RemovePaths,    this, &MythHTTPServer::StalePaths);
    connect(this, &MythHTTPServer::AddHandlers,    this, &MythHTTPServer::NewHandlers);
    connect(this, &MythHTTPServer::RemoveHandlers, this, &MythHTTPServer::StaleHandlers);
    connect(this, &MythHTTPServer::AddServices,    this, &MythHTTPServer::NewServices);
    connect(this, &MythHTTPServer::RemoveServices, this, &MythHTTPServer::StaleServices);
    connect(this, &MythHTTPServer::MasterResolved, this, &MythHTTPServer::ResolveMaster);
    connect(this, &MythHTTPServer::HostResolved,   this, &MythHTTPServer::ResolveHost);
    connect(this, &MythHTTPServer::AddErrorPageHandler, this, & MythHTTPServer::NewErrorPageHandler);
    connect(this, &MythHTTPServer::ProcessTCPQueue, this, & MythHTTPServer::ProcessTCPQueueHandler);

    // Find our static content
    m_config.m_rootDir = GetShareDir();
    while (m_config.m_rootDir.endsWith("/"))
        m_config.m_rootDir.chop(1);
    m_config.m_rootDir.append(QStringLiteral("/html"));

    // Add our default paths (mostly static js, css, images etc).
    // We need to pass individual directories to the threads, so inspect the
    // the paths we want for sub-directories
    static const QStringList s_dirs = { "/assets/", "/3rdParty/", "/css/", "/images/", "/js/", "/misc/", "/apps/", "/xslt/" };
    m_config.m_filePaths.clear();
    QStringList dirs;
    for (const auto & dir : s_dirs)
    {
        dirs.append(dir);
        QDirIterator it(m_config.m_rootDir + dir, QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext())
            dirs.append(it.next().remove(m_config.m_rootDir) + "/");
    }

    // And finally the root handler
    dirs.append("/");
    NewPaths(dirs);
}

MythHTTPServer::~MythHTTPServer()
{
    Stopped();
}

void MythHTTPServer::EnableDisable(bool Enable)
{
    if (Enable && !isListening())
    {
        Init();
        bool tcp = m_config.m_port != 0;
        bool ssl = m_config.m_sslPort != 0;

        if (tcp)
        {
            tcp = listen(m_config.m_port);
            // ServerPool as written will overwrite the port setting if we listen
            // on an additional port (i.e. SSL). So check which port is in use before
            // continuing
            if (m_config.m_port != serverPort())
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Server is using port %1 - expected %2")
                    .arg(serverPort()).arg(m_config.m_port));
                m_config.m_port = serverPort();
            }
            if (m_config.m_port_2)
                listen(m_config.m_port_2);
        }

        if (ssl)
        {
            ssl = listen(m_config.m_sslPort, true, kSSLServer);
            if (m_config.m_sslPort != serverPort())
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Server is using port %1 - expected %2")
                    .arg(serverPort()).arg(m_config.m_sslPort));
                m_config.m_sslPort = serverPort();
            }
        }

        Started(tcp, ssl);
    }
    else if (!Enable && isListening())
    {
        close();
        Stopped();
    }
}

/*! \brief Initialise server configuration
 *
 * This is called before we ask the server to start listening, which allows us
 * to stop and restart the server with different settings (new port etc)
*/
void MythHTTPServer::Init()
{
    // Just in case we get in a mess
    Stopped();

    // Decide on the ports to use
    if (gCoreContext->IsFrontend())
    {
        m_config.m_port = XmlConfiguration().GetValue("UPnP/MythFrontend/ServicePort", 6547);
        // I don't think there is an existing setting for this
        m_config.m_sslPort = static_cast<uint16_t>(gCoreContext->GetNumSetting("FrontendSSLPort", m_config.m_port + 10));

    }
    else if (gCoreContext->IsBackend())
    {
        m_config.m_port    = static_cast<uint16_t>(gCoreContext->GetBackendStatusPort());
        // Additional port, may be removed later
        m_config.m_port_2  = m_config.m_port + 200;
        m_config.m_sslPort = static_cast<uint16_t>(gCoreContext->GetNumSetting("BackendSSLPort", m_config.m_port + 10));
    }
    else
    {
        // N.B. This assumes only the frontend and backend use HTTP...
        m_config.m_port = 0;
        m_config.m_sslPort = 0;
    }

    // If this fails, unset the SSL port
#ifndef QT_NO_OPENSSL
    if (!MythHTTPS::InitSSLServer(m_config.m_sslConfig))
#endif
    {
        m_config.m_sslPort = 0;
    }

    if (m_config.m_sslPort == 0)
        LOG(VB_HTTP, LOG_INFO, LOC + "SSL is disabled");

    // Set the server ident
    QString version = GetMythSourceVersion();
    if (version.startsWith("v"))
        version = version.right(version.length() - 1);

#ifdef _WIN32
    QString server = QStringLiteral("Windows");
#else
    struct utsname uname_info {};
    uname(&uname_info);
    QString server = QStringLiteral("%1/%2").arg(uname_info.sysname, uname_info.release);
#endif
    m_config.m_serverName = QString("MythTV/%1 %2 UPnP/1.0").arg(version, server);

    // Retrieve language
    m_config.m_language = gCoreContext->GetLanguageAndVariant().replace("_", "-");

    // Get master backend details for Origin checks
    m_masterIPAddress  = gCoreContext->GetMasterServerIP();
    m_masterStatusPort = gCoreContext->GetMasterServerStatusPort();
    m_masterSSLPort    = gCoreContext->GetNumSetting("BackendSSLPort", m_masterStatusPort + 10);

    // Get keep alive timeout
    auto timeout = gCoreContext->GetNumSetting("HTTP/KeepAliveTimeoutSecs", HTTP_SOCKET_TIMEOUT_MS / 1000);
    m_config.m_timeout = static_cast<std::chrono::milliseconds>(timeout * 1000);
}

void MythHTTPServer::Started([[maybe_unused]] bool Tcp,
                             [[maybe_unused]] bool Ssl)
{
#if CONFIG_LIBDNS_SD
    // Advertise our webserver
    delete m_bonjour;
    delete m_bonjourSSL;
    m_bonjour = nullptr;
    m_bonjourSSL = nullptr;
    if (!(Tcp || Ssl))
        return;

    auto host = QHostInfo::localHostName();
    if (host.isEmpty())
        host = tr("Unknown");

    if (Tcp)
    {
        m_bonjour = new BonjourRegister();
        m_bonjour->Register(m_config.m_port, QByteArrayLiteral("_http._tcp"),
            QStringLiteral("%1 on %2").arg(QCoreApplication::applicationName(), host).toLatin1().constData(), {});
    }

    if (Ssl)
    {
        m_bonjourSSL = new BonjourRegister();
        m_bonjourSSL->Register(m_config.m_sslPort, QByteArrayLiteral("_https._tcp"),
            QStringLiteral("%1 on %2").arg(QCoreApplication::applicationName(), host).toLatin1().constData(), {});
    }
#endif

    // Build our list of hosts and allowed origins.
    BuildHosts();
    BuildOrigins();
}

void MythHTTPServer::Stopped()
{
    // Clear allowed origins
    m_config.m_hosts.clear();
    m_config.m_allowedOrigins.clear();

#if CONFIG_LIBDNS_SD
    // Stop advertising
    delete m_bonjour;
    delete m_bonjourSSL;
    m_bonjour = nullptr;
    m_bonjourSSL = nullptr;
#endif
}

void MythHTTPServer::ThreadFinished()
{
    if (!m_connectionQueue.empty())
    {
        emit ProcessTCPQueue();
    }
}

void MythHTTPServer::ProcessTCPQueueHandler()
{
    if (AvailableThreads() > 0)
    {
        auto entry = m_connectionQueue.dequeue();
        m_threadNum = m_threadNum % MaxThreads();
        auto name = QString("HTTP%1%2").arg(entry.m_ssl ? "S" : "").arg(m_threadNum++);
        auto * newthread = new MythHTTPThread(this, m_config, name, entry.m_socketFD, entry.m_ssl);
        AddThread(newthread);
        connect(newthread->qthread(), &QThread::finished, this, &MythHTTPThreadPool::ThreadFinished);
        connect(newthread->qthread(), &QThread::finished, this, &MythHTTPServer::ThreadFinished);
        newthread->start();
        return;
    }
}

void MythHTTPServer::newTcpConnection(qintptr Socket)
{
    if (!Socket)
        return;
    auto * server = qobject_cast<PrivTcpServer*>(QObject::sender());
    MythTcpQueueEntry entry;
    entry.m_socketFD = Socket;
    entry.m_ssl = (server->GetServerType() == kSSLServer);
    m_connectionQueue.enqueue(entry);
    emit ProcessTCPQueue();
}

bool MythHTTPServer::ReservedPath(const QString& Path)
{
    static const QStringList s_reservedPaths { HTTP_SERVICES_DIR };
    if (s_reservedPaths.contains(Path, Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Server path '%1' is reserved - ignoring").arg(Path));
        return true;
    }
    return false;
}

/*! \brief Add new paths that will serve simple files.
 *
 * File paths should not be duplicated (as it will reduce performance) but this
 * function will only warn when an attempt is made to add a path for a second time.
 * This ensures the server's behaviour does not break if a duplicated path is removed
 * dynamically (i.e. before the server quits).
 *
 * File paths are handled after all other handlers and are only called if another
 * handler for the given path does not provide a response. This allows a dynamic
 * handler to process specific requests for the given path, falling back to a
 * regular file handler for anything it does not recognise.
 *
 * Paths are not recursive and each directory must be added explicitly
 * (e.g. "/images/" will only serve files from the /images/ directory and not
 * /images/icons/). This improves performance and adds an additional modicum of security.
 *
 * It is assumed file paths are largely used for static server content (e.g. css,
 * images etc). As a result, caching defaults to 'long life', authorisation is never
 * requested and there is no filtering of files (i.e. any request for a readable
 * file in the given directory will result in the file being sent).
*/
void MythHTTPServer::NewPaths(const QStringList &Paths)
{
    if (Paths.isEmpty())
        return;
    for (const auto & path : std::as_const(Paths))
    {
        if (ReservedPath(path))
            continue;
        if (m_config.m_filePaths.contains(path))
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("'%1' is already registered").arg(path));
        else
            LOG(VB_HTTP, LOG_INFO, LOC + QString("Adding path: '%1'").arg(path));
        m_config.m_filePaths.append(path);
    }
    emit PathsChanged(m_config.m_filePaths);
}

void MythHTTPServer::StalePaths(const QStringList& Paths)
{
    if (Paths.isEmpty())
        return;
    for (const auto & path : std::as_const(Paths))
    {
        if (m_config.m_filePaths.contains(path))
        {
            LOG(VB_HTTP, LOG_INFO, LOC + QString("Removing path: '%1'").arg(path));
            m_config.m_filePaths.removeOne(path);
        }
    }
    emit PathsChanged(m_config.m_filePaths);
}

/*! \brief Add new handlers.
 *
 * Handlers allow dynamic content to be served for the given path.
 *
 * The underlying function reference is a std::function. This allows the handler
 * to be a lambda, a functor, a static member function or a full blown member function
 * - all with parameter re-ordering, defaults etc.
 *
 * It should be noted that the supplied function will be called from multiple
 * threads, so the code must ensure thread safety. The webserver code attempts
 * to maximise concurrency (and hence responsiveness) by avoiding blocking the
 * socket thread. It is expected that individual handlers will attempt to do the
 * same.
 *
 * As for regular file paths, handlers paths are not recursive.
 *
 * \note We do not allow duplication of paths with handlers as there is no way
 * to compare std::function for equality - hence we cannot differentiate between
 * std::pair<"/",FuncXXX> and std::pair<"/",FuncYYY> and hence cannot remove
 * a handler unambiguously.
*/
void MythHTTPServer::NewHandlers(const HTTPHandlers& Handlers)
{
    bool newhandlers = false;
    for (const auto & handler : std::as_const(Handlers))
    {
        if (ReservedPath(handler.first))
            continue;
        if (!std::any_of(m_config.m_handlers.cbegin(), m_config.m_handlers.cend(),
                         [&handler](const HTTPHandler& Handler) { return Handler.first == handler.first; }))
        {
            LOG(VB_HTTP, LOG_INFO, LOC + QString("Adding handler for '%1'").arg(handler.first));
            m_config.m_handlers.push_back(handler);
            newhandlers = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Handler '%1' already registered - ignoring")
                .arg(handler.first));
        }
    }
    if (newhandlers)
        emit HandlersChanged(m_config.m_handlers);
}

void MythHTTPServer::StaleHandlers(const HTTPHandlers& Handlers)
{
    bool stalehandlers = false;
    for (const auto & handler : std::as_const(Handlers))
    {
        auto found = std::find_if(m_config.m_handlers.begin(), m_config.m_handlers.end(),
                                  [&handler](const HTTPHandler& Handler) {  return Handler.first == handler.first; });
        if (found != m_config.m_handlers.end())
        {
            m_config.m_handlers.erase(found);
            stalehandlers = true;
        }
    }
    if (stalehandlers)
        emit HandlersChanged(m_config.m_handlers);
}

void MythHTTPServer::NewServices(const HTTPServices& Services)
{
    bool newservices = false;
    for (const auto & service : std::as_const(Services))
    {
        if (ReservedPath(service.first))
            continue;
        if (!std::any_of(m_config.m_services.cbegin(), m_config.m_services.cend(),
                         [&service](const HTTPService& Service) { return Service.first == service.first; }))
        {
            LOG(VB_HTTP, LOG_INFO, LOC + QString("Adding service for '%1'").arg(service.first));
            m_config.m_services.push_back(service);
            newservices = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Service '%1' already registered - ignoring")
                .arg(service.first));
        }
    }
    if (newservices)
        emit ServicesChanged(m_config.m_services);
}

void MythHTTPServer::StaleServices(const HTTPServices& Services)
{
    bool staleservices = false;
    for (const auto & service : std::as_const(Services))
    {
        auto found = std::find_if(m_config.m_services.begin(), m_config.m_services.end(),
                                  [&service](const HTTPService& Service) {  return Service.first == service.first; });
        if (found != m_config.m_services.end())
        {
            m_config.m_services.erase(found);
            staleservices = true;
        }
    }
    if (staleservices)
        emit ServicesChanged(m_config.m_services);
}

/*! \brief Add new error page handler.
 *
 * Handler to return error page when no other content is found.
 *
 * Used for single page web apps (eg. backend web app)
 *
*/
void MythHTTPServer::NewErrorPageHandler(const HTTPHandler& Handler)
{
    LOG(VB_HTTP, LOG_INFO, LOC + QString("Adding error page handler"));
    m_config.m_errorPageHandler = Handler;
    emit ErrorHandlerChanged(m_config.m_errorPageHandler);
}

void MythHTTPServer::BuildHosts()
{
    m_config.m_hosts.clear();

    // Iterate over the addresses ServerPool was asked to listen on. This should
    // pick up all variations of localhost and external IP etc
    QStringList lookups;
    auto defaults = DefaultListen();
    bool allipv4 = false;
    bool allipv6 = false;
    for (const auto & address : std::as_const(defaults))
    {
        if (address == QHostAddress::AnyIPv4)
            allipv4 |= true;
        else if (address == QHostAddress::AnyIPv6)
            allipv6 |= true;
        else
            lookups.append(address.toString());
    }

    // 'Any address' (0.0.0.0) is in use. Retrieve the complete list of avaible
    // addresses and filter as required.
    if (allipv4 || allipv6)
    {
        auto addresses = QNetworkInterface::allAddresses();
        for (const auto & address : std::as_const(addresses))
        {
            if ((allipv4 && address.protocol() == QAbstractSocket::IPv4Protocol) ||
                (allipv6 && address.protocol() ==  QAbstractSocket::IPv6Protocol))
            {
                lookups.append(address.toString());
            }
        }
    }

    lookups.removeDuplicates();

    // Trigger reverse lookups
    for (const auto & address : lookups)
    {
        m_hostLookups++;
        QHostInfo::lookupHost(address, this, &MythHTTPServer::HostResolved);
    }
}

/*! \brief Generate a list of allowed 'Origins' for validating CORS requests.
 *
 * This is not as straightforward as it might first appear.
 * We need the IP addresses AND host names for all origins that are deemed
 * safe. This is limited to the current machine (for addresses/interfaces that
 * ServerPool is listening on), the master backend and any additional overrides
 * from the 'AllowedOriginsList' setting. Furthermore the port is optional
 * and there will be the usual additional complications for IPv6 addresses...
 *
 * \note DO NOT USE the 'Access-Control-Allow-Origin' wildcard.
*/
void MythHTTPServer::BuildOrigins()
{
    m_config.m_allowedOrigins.clear();

    // Add master backend. We need to resolve this separately to handle both status
    // and SSL ports
    m_originLookups++;
    QHostInfo::lookupHost(m_masterIPAddress, this, &MythHTTPServer::MasterResolved);

    // Add configured overrides - are these still needed?
    QStringList extras = gCoreContext->GetSetting("AllowedOriginsList", QString(
                                                  "https://chromecast.mythtv.org"
                                                  )).split(",");
    for (const auto & extra : std::as_const(extras))
    {
        QString clean = extra.trimmed();
        if (clean.startsWith("http://") || clean.startsWith("https://"))
            m_config.m_allowedOrigins.append(clean);
    }
}

QStringList MythHTTPServer::BuildAddressList(QHostInfo& Info)
{
    bool addhostname = true;
    QString hostname = Info.hostName();
    QStringList results;
    auto ipaddresses = Info.addresses();
    for(auto & address : ipaddresses)
    {
        QString result = MythHTTP::AddressToString(address);
        // This filters out IPv6 addresses that are passed back as host names
        if (result.contains(hostname))
            addhostname = false;
        results.append(result.toLower());
    }
    if (addhostname)
        results.append(hostname.toLower());
    return results;
}

/*! \brief Add master backend addresses to the allowed Origins list.
 *
 * The master backend *may* be using both http and https so we need to account
 * for both here.
*/
void MythHTTPServer::ResolveMaster(QHostInfo Info)
{
    auto addresses = BuildAddressList(Info);

    // Add status and SSL addressed for each
    for (const auto & address : std::as_const(addresses))
    {
        m_config.m_allowedOrigins.append(QString("http://%1").arg(address));
        m_config.m_allowedOrigins.append(QString("http://%1:%2").arg(address).arg(m_masterStatusPort));
        if (m_masterSSLPort != 0)
        {
            m_config.m_allowedOrigins.append(QString("https://%1").arg(address));
            m_config.m_allowedOrigins.append(QString("https://%1:%2").arg(address).arg(m_masterSSLPort));
        }
    }
    m_config.m_allowedOrigins.removeDuplicates();
    if (--m_originLookups == 0)
        DebugOrigins();
    emit OriginsChanged(m_config.m_allowedOrigins);
}

void MythHTTPServer::DebugOrigins()
{
    if (VERBOSE_LEVEL_CHECK(VB_HTTP, LOG_INFO))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Name resolution complete: %1 'Origins' found").arg(m_config.m_allowedOrigins.size()));
        for (const auto & address : std::as_const(m_config.m_allowedOrigins))
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Allowed origin: %1").arg(address));
    }
}

/*! \brief Add the results of a reverse lookup to our allowed list.
*/
void MythHTTPServer::ResolveHost(QHostInfo Info)
{
    auto addresses = BuildAddressList(Info);
    for (const auto & address : std::as_const(addresses))
    {
        // The port is optional - so just add both to our list to simplify the
        // checks when a request is received
        m_config.m_hosts.append(QString("%1").arg(address));
        if (m_config.m_port != 0)
            m_config.m_hosts.append(QString("%1:%2").arg(address).arg(m_config.m_port));
        if (m_config.m_sslPort != 0)
            m_config.m_hosts.append(QString("%1:%2").arg(address).arg(m_config.m_sslPort));
    }
    m_config.m_hosts.removeDuplicates();
    if (--m_hostLookups == 0)
        DebugHosts();
    emit HostsChanged(m_config.m_hosts);
}

void MythHTTPServer::DebugHosts()
{
    if (VERBOSE_LEVEL_CHECK(VB_HTTP, LOG_INFO))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Name resolution complete: %1 'Hosts' found").arg(m_config.m_hosts.size()));
        for (const auto & address : std::as_const(m_config.m_hosts))
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Host: %1").arg(address));
    }
}
