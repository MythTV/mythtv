#ifndef MYTHHTTPPARSER_H
#define MYTHHTTPPARSER_H

// MythTV
#include "libmythbase/http/mythhttptypes.h"

class QTcpSocket;

class MythHTTPParser
{
  public:
    MythHTTPParser() = default;
    bool Read(QTcpSocket* Socket, bool& Ready);
    HTTPRequest2 GetRequest(const MythHTTPConfig& Config, QTcpSocket* Socket);

  private:
    Q_DISABLE_COPY(MythHTTPParser)

    bool        m_started { false };
    int         m_linesRead { 0 };
    bool        m_headersComplete { false };
    QString     m_method;
    HTTPHeaders m_headers { std::make_shared<HTTPMap>() };
    int64_t     m_contentLength { 0 };
    HTTPData    m_content { nullptr };
};

#endif
