#ifndef MYTHHTTPREQUEST_H
#define MYTHHTTPREQUEST_H

// Qt
#include <QUrl>

// MythTV
#include "libmythbase/http/mythhttptypes.h"

class QTcpSocket;

class MBASE_PUBLIC MythHTTPRequest
{
  public:
    MythHTTPRequest(const MythHTTPConfig& Config, QString Method,
                    HTTPHeaders Headers, HTTPData Content, QTcpSocket* Socket = nullptr);

    QString     m_serverName;
    QString     m_method;
    HTTPHeaders m_headers { nullptr };
    HTTPData    m_content { nullptr };

    MythHTTPStatus      m_status     { HTTPBadRequest };
    QUrl                m_url;
    QString             m_root;
    QString             m_path;
    QString             m_fileName;
    HTTPQueries         m_queries;
    MythHTTPVersion     m_version    { HTTPUnknownVersion };
    MythHTTPRequestType m_type       { HTTPUnknown };
    MythHTTPConnection  m_connection { HTTPConnectionClose };
    std::chrono::milliseconds m_timeout { HTTP_SOCKET_TIMEOUT_MS };
    int                 m_allowed    { HTTP_DEFAULT_ALLOWED };
    QHostAddress        m_peerAddress;

  private:
    Q_DISABLE_COPY(MythHTTPRequest)
    static HTTPQueries ParseQuery(const QString& Query);
};

#endif
