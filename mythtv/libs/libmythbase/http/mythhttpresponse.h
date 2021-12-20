#ifndef MYTHHTTPRESPONSE_H
#define MYTHHTTPRESPONSE_H

// MythTV
#include "http/mythhttpfile.h"
#include "http/mythwebsockettypes.h"
#include "http/mythhttptypes.h"
#include "http/mythhttpdata.h"
#include "http/mythhttprequest.h"

// Std
#include <vector>

class QTcpSocket;

class MBASE_PUBLIC MythHTTPResponse
{
  public:
    MythHTTPResponse() = default;
    explicit MythHTTPResponse(const HTTPRequest2 Request);

    static HTTPResponse HandleOptions       (HTTPRequest2 Request);
    static HTTPResponse ErrorResponse       (MythHTTPStatus Status, const QString& ServerName);
    static HTTPResponse RedirectionResponse (HTTPRequest2 Request, const QString& Redirect);
    static HTTPResponse ErrorResponse       (HTTPRequest2 Request, const QString& Message = {});
    static HTTPResponse OptionsResponse     (HTTPRequest2 Request);
    static HTTPResponse DataResponse        (HTTPRequest2 Request, HTTPData Data);
    static HTTPResponse FileResponse        (HTTPRequest2 Request, HTTPFile File);
    static HTTPResponse EmptyResponse       (HTTPRequest2 Request);
    static HTTPResponse UpgradeResponse     (HTTPRequest2 Request, MythSocketProtocol& Protocol, bool& Testing);

    void Finalise  (const MythHTTPConfig& Config);
    template <class T>
    typename std::enable_if<std::is_convertible<T, QString>::value
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
                         && !std::is_same<T, QByteArray>::value
#endif
        , void>::type
    AddHeader (const QString& key, const T& val)
    {
        QByteArray bytes = QString("%1: %2\r\n").arg(key, val).toUtf8();
        m_responseHeaders.emplace_back(MythHTTPData::Create(bytes));
    }
    template <class T>
    typename std::enable_if<!std::is_convertible<T, QString>::value, void>::type
    AddHeader (const QString& key, T val)
    {
        QByteArray bytes = QString("%1: %2\r\n").arg(key).arg(val).toUtf8();
        m_responseHeaders.emplace_back(MythHTTPData::Create(bytes));
    }

    QString             m_serverName;
    MythHTTPVersion     m_version         { HTTPOneDotOne };
    MythHTTPConnection  m_connection      { HTTPConnectionClose };
    std::chrono::milliseconds m_timeout   { HTTP_SOCKET_TIMEOUT_MS };
    MythHTTPStatus      m_status          { HTTPBadRequest };
    MythHTTPRequestType m_requestType     { HTTPGet };
    int                 m_allowed         { HTTP_DEFAULT_ALLOWED };
    HTTPHeaders         m_requestHeaders  { nullptr };
    HTTPContents        m_responseHeaders { };
    HTTPVariant         m_response        { std::monostate() };

  protected:
    void AddDefaultHeaders();
    void AddContentHeaders();

  private:
    Q_DISABLE_COPY(MythHTTPResponse)
};

#endif
