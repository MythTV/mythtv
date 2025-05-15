// Qt
#include <QHash>
#include <QString>
#include <QTcpSocket>

// MythTV
#include "mythlogging.h"
#include "http/mythhttpdata.h"
#include "http/mythhttpencoding.h"
#include "http/mythhttprequest.h"

#define LOC QString("HTTPParse: ")

/*! \class MythHTTPRequest
 * \brief Limited parsing of HTTP method and some headers to determine validity of request.
 *
 * The aim here is to parse the minimum amount of data to determine whether the
 * request should be processed and, if not, set enough state to send the appropriate
 * error response.
 *
 * \note If parsing fails early, the connection type remains at 'close' and hence
 * the socket will be closed once the error response is sent.
*/
MythHTTPRequest::MythHTTPRequest(const MythHTTPConfig& Config, QString Method,
                                 HTTPHeaders Headers, HTTPData Content, QTcpSocket* Socket /*=nullptr*/)
  : m_serverName(Config.m_serverName),
    m_method(std::move(Method)),
    m_headers(std::move(Headers)),
    m_content(std::move(Content)),
    m_root(Config.m_rootDir),
    m_timeout(Config.m_timeout)
{
    // TODO is the simplified() call here always safe?
    QStringList tokens = m_method.simplified().split(' ', Qt::SkipEmptyParts);

    // Validation
    // Must have verb and url and optional version
    if (tokens.size() < 2 || tokens.size() > 3)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to parse HTTP method");
        return;
    }

    // Note
    // tokens[0] = GET, POST, etc
    // tokens[1] = rest of URL
    // tokens[2] = HTTP/1.1

    m_type   = MythHTTP::RequestFromString(tokens[0]);
    m_url = tokens[1];

    // If no version, assume HTTP/1.1
    m_version = HTTPOneDotOne;
    if (tokens.size() > 2)
        m_version = MythHTTP::VersionFromString(tokens[2]);

    // Unknown HTTP version
    if (m_version == HTTPUnknownVersion)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Unknown HTTP version");
        m_version = HTTPOneDotOne;
        return;
    }

    // Unknown request type
    if (m_type == HTTPUnknown)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Unknown HTTP request");
        return;
    }

    // HTTP/1.1 requires the HOST header - even if empty.
    bool havehost = m_headers->contains("host");
    if ((m_version == HTTPOneDotOne) && !havehost)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "No host header for HTTP/1.1");
        return;
    }

    // Multiple host headers are also forbidden - assume for any version not just 1/1
    if (havehost && m_headers->count("host") > 1)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Multiple 'Host' headers forbidden");
        return;
    }

    // If a host is provided, ensure we recognise it. This may be over zealous:)
    if (havehost)
    {

        // Commented this check because people should be able to set up something in their
        // hosts file that does not match the server. Then the host name in the request
        // may not match.

        // N.B. host port is optional - but our host list has both versions
        // QString host = MythHTTP::GetHeader(m_headers, "host").toLower();
        // QStringList hostParts = host.split(":");
        // if (!Config.m_hosts.contains(hostParts[0]))
        // {
        //     LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Invalid 'Host' header. '%1' not recognised")
        //         .arg(host));
        //     return;
        // }

        // TODO Ensure the host address has a port - as below when we add one manually
    }
    else if (Socket)
    {
        // Use the socket address to add a host address. This just ensures the
        // response always has a valid address for this thread/socket that can be used
        // when building a (somewhat dynamic) response.
        QHostAddress host = Socket->localAddress();
        m_headers->insert("host", QString("%1:%2").arg(MythHTTP::AddressToString(host)).arg(Socket->localPort()));
    }

    if (Socket)
        m_peerAddress = Socket->peerAddress();

    // Need a valid URL
    if (!m_url.isValid())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Invalid URL: '%1'").arg(m_url.toString()));
        return;
    }

    // Parse the URL into its useful components (path/filename) - queries later
    m_path     = m_url.toString(QUrl::RemoveFilename | QUrl::RemoveFragment | QUrl::RemoveQuery);
    m_fileName = m_url.fileName();

    // Parse the connection header
    // HTTP/1.1 default to KeepAlive, HTTP/1.0 default to close
    // HTTP/0.9 is unlikely but assume KeepAlive
    m_connection = (m_version == HTTPOneDotZero) ? HTTPConnectionClose : HTTPConnectionKeepAlive;
    auto connection = MythHTTP::GetHeader(m_headers, "connection").toLower();
    if (connection.contains(QStringLiteral("keep-alive")))
        m_connection = HTTPConnectionKeepAlive;
    else if (connection.contains(QStringLiteral("close")))
        m_connection = HTTPConnectionClose;

    // Parse the content type if present - and pull out any form data
    if (m_content.get() && !m_content->isEmpty() && ((m_type == HTTPPut) || (m_type == HTTPPost)))
        MythHTTPEncoding::GetContentType(this);

    // Only parse queries if we do not have form data
    if (m_queries.isEmpty() && m_url.hasQuery())
        m_queries = ParseQuery(m_url.query());

    m_status = HTTPOK;
}

HTTPQueries MythHTTPRequest::ParseQuery(const QString &Query)
{
    HTTPQueries result;
    QStringList params = Query.split('&', Qt::SkipEmptyParts);
    for (const auto & param : std::as_const(params))
    {
        QString key   = param.section('=', 0, 0);
        QString value = param.section('=', 1);
        QByteArray rawvalue = value.toUtf8();
        value = QUrl::fromPercentEncoding(rawvalue);
        value.replace("+", " ");
        if (!key.isEmpty())
            result.insert(key.trimmed().toLower(), value);
    }
    return result;
}
