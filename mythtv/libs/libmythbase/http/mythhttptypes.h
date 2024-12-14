#ifndef MYTHHTTPTYPES_H
#define MYTHHTTPTYPES_H

// Std
#include <array>
#include <deque>
#include <chrono>

// Qt
#include <QMap>
#include <QFile>
#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QHostAddress>
#ifndef QT_NO_OPENSSL
#include <QSslConfiguration>
#endif
#include <QTimeZone>

// MythTV
#include "libmythbase/http/mythhttpcommon.h"
#include "libmythbase/http/mythmimetype.h"

#define HTTP_SOCKET_TIMEOUT_MS 10000 // 10 seconds
#define HTTP_SERVICES_DIR QString("/services/")

class MythHTTPData;
class MythHTTPFile;
class MythHTTPRequest;
class MythHTTPResponse;
class MythHTTPService;

using HTTPMap           = QMultiMap<QString,QString>;
using HTTPQueries       = HTTPMap;
using HTTPHeaders       = std::shared_ptr<HTTPMap>;
using HTTPData          = std::shared_ptr<MythHTTPData>;
using HTTPContents      = std::vector<HTTPData>;
using HTTPRequest2      = std::shared_ptr<MythHTTPRequest>; // HTTPRequest conflicts with existing class
using HTTPResponse      = std::shared_ptr<MythHTTPResponse>;
using HTTPFile          = std::shared_ptr<MythHTTPFile>;
using HTTPVariant       = std::variant<std::monostate, HTTPData, HTTPFile>;
using HTTPQueue         = std::deque<HTTPVariant>;
using HTTPRange         = std::pair<uint64_t,uint64_t>;
using HTTPRanges        = std::vector<HTTPRange>;
using HTTPFunction      = std::function<HTTPResponse(HTTPRequest2)>;
using HTTPHandler       = std::pair<QString,HTTPFunction>;
using HTTPHandlers      = std::vector<HTTPHandler>;
using HTTPMulti         = std::pair<HTTPData,HTTPData>;
using HTTPRegisterTypes = std::function<void()>;
using HTTPServicePtr    = std::shared_ptr<MythHTTPService>;
using HTTPServicePtrs   = std::vector<HTTPServicePtr>;
using HTTPServiceCtor   = std::function<HTTPServicePtr()>;
using HTTPService       = std::pair<QString,HTTPServiceCtor>;
using HTTPServices      = std::vector<HTTPService>;

Q_DECLARE_METATYPE(HTTPHandler)
Q_DECLARE_METATYPE(HTTPHandlers)
Q_DECLARE_METATYPE(HTTPServices)

class MythHTTPConfig
{
  public:
    quint16      m_port    { 0 };
    quint16      m_port_2  { 0 };
    quint16      m_sslPort { 0 };
    QString      m_rootDir;
    QString      m_serverName;
    std::chrono::milliseconds m_timeout { HTTP_SOCKET_TIMEOUT_MS };
    QString      m_language;
    QStringList  m_hosts;
    QStringList  m_allowedOrigins;
    QStringList  m_filePaths;
    HTTPHandlers m_handlers;
    HTTPServices m_services;
    HTTPHandler  m_errorPageHandler;
#ifndef QT_NO_OPENSSL
    QSslConfiguration m_sslConfig;
#endif
};

enum MythHTTPVersion
{
    HTTPUnknownVersion = 0,
    HTTPZeroDotNine,
    HTTPOneDotZero,
    HTTPOneDotOne
};

enum MythHTTPRequestType
{
    HTTPUnknown = 0x0000,
    HTTPHead    = 0x0001,
    HTTPGet     = 0x0002,
    HTTPPost    = 0x0004,
    HTTPPut     = 0x0008,
    HTTPDelete  = 0x0010,
    HTTPOptions = 0x0020
};

// POST is needed here because all services are invoked with POST when using
// SOAP, regardless of whether they require POST or not
#define HTTP_DEFAULT_ALLOWED (HTTPHead | HTTPGet | HTTPOptions | HTTPPost)

enum MythHTTPStatus
{
    HTTPSwitchingProtocols  = 101,
    HTTPOK                  = 200,
    HTTPAccepted            = 202,
    HTTPPartialContent      = 206,
    HTTPMovedPermanently    = 301,
    HTTPNotModified         = 304,
    HTTPBadRequest          = 400,
    HTTPUnauthorized        = 401,
    HTTPForbidden           = 403,
    HTTPNotFound            = 404,
    HTTPMethodNotAllowed    = 405,
    HTTPRequestTimeout      = 408,
    HTTPRequestedRangeNotSatisfiable = 416,
    HTTPTooManyRequests     = 429,
    HTTPInternalServerError = 500,
    HTTPNotImplemented      = 501,
    HTTPBadGateway          = 502,
    HTTPServiceUnavailable  = 503,
    HTTPNetworkAuthenticationRequired = 511
};

enum MythHTTPConnection
{
    HTTPConnectionClose     = 0,
    HTTPConnectionKeepAlive,
    HTTPConnectionUpgrade
};

enum MythHTTPEncode
{
    HTTPNoEncode = 0,
    HTTPGzip,
    HTTPChunked
};

enum MythHTTPCacheType
{
    HTTPIgnoreCache  = 0x0000,
    HTTPNoCache      = 0x0001,
    HTTPETag         = 0x0002,
    HTTPLastModified = 0x0004,
    HTTPShortLife    = 0x0100,
    HTTPMediumLife   = 0x0200,
    HTTPLongLife     = 0x0400
};

static QString s_multipartBoundary = "mYtHtVxXxXxX";
static QString s_defaultHTTPPage =
"<!DOCTYPE html>"
"<HTML>"
  "<HEAD>"
    "<TITLE>%1</TITLE>"
    "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">"
  "</HEAD>"
  "<BODY><H1>%1</H1></BODY>"
"</HTML>";

class MythHTTPContent
{
  public:
    explicit MythHTTPContent(QString Name): m_fileName(std::move(Name)) {}
    QString        m_fileName;
    HTTPRanges     m_ranges;
    int64_t        m_partialSize { 0 };
    HTTPContents   m_multipartHeaders;
    int64_t        m_multipartHeaderSize { 0 };
    int64_t        m_written  { 0 };
    MythHTTPEncode m_encoding { HTTPNoEncode };
    MythMimeType   m_mimeType { };
    QDateTime      m_lastModified;
    QByteArray     m_etag;
    int            m_cacheType { HTTPIgnoreCache };
};

class MythHTTP
{
  public:
    static QString VersionToString(MythHTTPVersion Version)
    {
        switch (Version)
        {
            case HTTPOneDotOne:      return QStringLiteral("HTTP/1.1");
            case HTTPOneDotZero:     return QStringLiteral("HTTP/1.0");
            case HTTPZeroDotNine:    return QStringLiteral("HTTP/0.9");
            case HTTPUnknownVersion: break;
        }
        return QStringLiteral("Error");
    }

    static MythHTTPVersion VersionFromString(const QString& Version)
    {
        if (Version.startsWith(QStringLiteral("HTTP")))
        {
            if (Version.endsWith(QStringLiteral("1.1"))) return HTTPOneDotOne;
            if (Version.endsWith(QStringLiteral("1.0"))) return HTTPOneDotZero;
            if (Version.endsWith(QStringLiteral("0.9"))) return HTTPZeroDotNine;
        }
        return HTTPUnknownVersion;
    }

    static QString RequestToString(MythHTTPRequestType Type)
    {
        switch (Type)
        {
            case HTTPHead:    return QStringLiteral("HEAD");
            case HTTPGet:     return QStringLiteral("GET");
            case HTTPPost:    return QStringLiteral("POST");
            case HTTPPut:     return QStringLiteral("PUT");
            case HTTPDelete:  return QStringLiteral("DELETE");
            case HTTPOptions: return QStringLiteral("OPTIONS");
            case HTTPUnknown: break;
        }
        return QStringLiteral("Error");
    }

    static MythHTTPRequestType RequestFromString(const QString& Type)
    {
        if (Type == QStringLiteral("GET"))     return HTTPGet;
        if (Type == QStringLiteral("HEAD"))    return HTTPHead;
        if (Type == QStringLiteral("POST"))    return HTTPPost;
        if (Type == QStringLiteral("PUT"))     return HTTPPut;
        if (Type == QStringLiteral("OPTIONS")) return HTTPOptions;
        if (Type == QStringLiteral("DELETE"))  return HTTPDelete;
        return HTTPUnknown;
    }

    static QString AllowedRequestsToString(int Allowed)
    {
        QStringList result;
        if (Allowed & HTTPHead)    result << QStringLiteral("HEAD");
        if (Allowed & HTTPGet)     result << QStringLiteral("GET");
        if (Allowed & HTTPPost)    result << QStringLiteral("POST");
        if (Allowed & HTTPPut)     result << QStringLiteral("PUT");
        if (Allowed & HTTPDelete)  result << QStringLiteral("DELETE");
        if (Allowed & HTTPOptions) result << QStringLiteral("OPTIONS");
        return result.join(QStringLiteral(", "));
    }

    static int RequestsFromString(const QString& Requests)
    {
        int result = HTTPUnknown;
        if (Requests.contains("HEAD",    Qt::CaseInsensitive)) result |= HTTPHead;
        if (Requests.contains("GET",     Qt::CaseInsensitive)) result |= HTTPGet;
        if (Requests.contains("POST",    Qt::CaseInsensitive)) result |= HTTPPost;
        if (Requests.contains("PUT",     Qt::CaseInsensitive)) result |= HTTPPut;
        if (Requests.contains("DELETE",  Qt::CaseInsensitive)) result |= HTTPDelete;
        if (Requests.contains("OPTIONS", Qt::CaseInsensitive)) result |= HTTPOptions;
        return result;
    }

    static QString StatusToString(MythHTTPStatus Status)
    {
        switch (Status)
        {
            case HTTPSwitchingProtocols:  return QStringLiteral("101 Switching Protocols");
            case HTTPOK:                  return QStringLiteral("200 OK");
            case HTTPAccepted:            return QStringLiteral("202 Accepted");
            case HTTPPartialContent:      return QStringLiteral("206 Partial Content");
            case HTTPMovedPermanently:    return QStringLiteral("301 Moved Permanently");
            case HTTPNotModified:         return QStringLiteral("304 Not Modified");
            case HTTPBadRequest:          return QStringLiteral("400 Bad Request");
            case HTTPUnauthorized:        return QStringLiteral("401 Unauthorized");
            case HTTPForbidden:           return QStringLiteral("403 Forbidden");
            case HTTPNotFound:            return QStringLiteral("404 Not Found");
            case HTTPMethodNotAllowed:    return QStringLiteral("405 Method Not Allowed");
            case HTTPRequestTimeout:      return QStringLiteral("408 Request Timeout");
            case HTTPRequestedRangeNotSatisfiable: return QStringLiteral("416 Requested Range Not Satisfiable");
            case HTTPTooManyRequests:     return QStringLiteral("429 Too Many Requests");
            case HTTPInternalServerError: return QStringLiteral("500 Internal Server Error");
            case HTTPNotImplemented:      return QStringLiteral("501 Not Implemented");
            case HTTPBadGateway:          return QStringLiteral("502 Bad Gateway");
            case HTTPServiceUnavailable:  return QStringLiteral("503 Service Unavailable");
            case HTTPNetworkAuthenticationRequired: return QStringLiteral("511 Network Authentication Required");
        }
        return QStringLiteral("Error");
    }

    static MythHTTPStatus StatusFromString(const QString& Status)
    {
        if (Status.startsWith(QStringLiteral("200"))) return HTTPOK;
        if (Status.startsWith(QStringLiteral("101"))) return HTTPSwitchingProtocols;
        if (Status.startsWith(QStringLiteral("206"))) return HTTPPartialContent;
        if (Status.startsWith(QStringLiteral("301"))) return HTTPMovedPermanently;
        if (Status.startsWith(QStringLiteral("304"))) return HTTPNotModified;
        //if (Status.startsWith(QStringLiteral("400"))) return HTTPBadRequest;
        if (Status.startsWith(QStringLiteral("401"))) return HTTPUnauthorized;
        if (Status.startsWith(QStringLiteral("403"))) return HTTPForbidden;
        if (Status.startsWith(QStringLiteral("404"))) return HTTPNotFound;
        if (Status.startsWith(QStringLiteral("405"))) return HTTPMethodNotAllowed;
        if (Status.startsWith(QStringLiteral("408"))) return HTTPRequestTimeout;
        if (Status.startsWith(QStringLiteral("416"))) return HTTPRequestedRangeNotSatisfiable;
        if (Status.startsWith(QStringLiteral("429"))) return HTTPTooManyRequests;
        if (Status.startsWith(QStringLiteral("500"))) return HTTPNotImplemented;
        if (Status.startsWith(QStringLiteral("501"))) return HTTPBadGateway;
        if (Status.startsWith(QStringLiteral("502"))) return HTTPServiceUnavailable;
        if (Status.startsWith(QStringLiteral("511"))) return HTTPNetworkAuthenticationRequired;
        return HTTPBadRequest;
    }

    static QString ConnectionToString(MythHTTPConnection Connection)
    {
        switch (Connection)
        {
            case HTTPConnectionClose:     return QStringLiteral("close");
            case HTTPConnectionKeepAlive: break;
            case HTTPConnectionUpgrade:   return QStringLiteral("Upgrade");
        }
        return QStringLiteral("keep-alive");
    }

    // N.B. Value must be lower case
    static QString GetHeader(const HTTPHeaders& Headers, const QString& Value, const QString& Default = "")
    {
        if (Headers && Headers->contains(Value))
            return Headers->value(Value);
        return Default;
    }

    static inline QString GetContentType(const MythMimeType& Mime)
    {
        QString type = Mime.Name();
        if (Mime.Inherits("text/plain"))
            type += "; charset=\"UTF-8\"";
        return type;
    }

    static inline QString AddressToString(QHostAddress& Address)
    {
        if (Address.protocol() == QAbstractSocket::IPv6Protocol)
        {
            Address.setScopeId("");
            return QString("[%1]").arg(Address.toString());
        }
        return Address.toString();
    }
};
#endif
