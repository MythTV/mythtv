// C++
#include <algorithm>

// Qt
#include <QTcpSocket>

// MythTV
#include "mythlogging.h"
#include "http/mythhttpdata.h"
#include "http/mythhttprequest.h"
#include "http/mythhttpparser.h"

#define LOC QString("HTTPParser: ")

HTTPRequest2 MythHTTPParser::GetRequest(const MythHTTPConfig& Config, QTcpSocket* Socket)
{
    // Debug
    if (VERBOSE_LEVEL_CHECK(VB_HTTP, LOG_DEBUG))
    {
        LOG(VB_HTTP, LOG_DEBUG, m_method);
        for (auto it = m_headers->cbegin(); it != m_headers->cend(); ++it)
            LOG(VB_HTTP, LOG_DEBUG, it.key() + ": " + it.value());
        if (m_content.get())
            LOG(VB_HTTP, LOG_DEBUG, QString("Content:\r\n") + m_content->constData());
    }

    // Build the request
    auto result = std::make_shared<MythHTTPRequest>(Config, m_method, m_headers, m_content, Socket);

    // Reset our internal state
    m_started = false;
    m_linesRead = 0;
    m_headersComplete = false;
    m_method.clear();
    m_contentLength = 0;
    m_headers = std::make_shared<HTTPMap>();
    m_content = nullptr;

    return result;
}

bool MythHTTPParser::Read(QTcpSocket* Socket, bool& Ready)
{
    Ready = false;

    // Fail early
    if (!Socket || (Socket->state() != QAbstractSocket::ConnectedState))
        return false;

    // Sanity check the number of headers
    if (!m_headersComplete && m_linesRead > 200)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Read %1 headers - aborting").arg(m_linesRead));
        return false;
    }

    // Filter out non HTTP content quickly. This assumes server side only.
    if (!m_started && Socket->bytesAvailable() > 2)
    {
        QByteArray buf(3, '\0');
        if (Socket->peek(buf.data(), 3) == 3)
        {
            static const std::vector<const char *> s_starters = { "GET", "PUT", "POS", "OPT", "HEA", "DEL" };
            if (!std::any_of(s_starters.cbegin(), s_starters.cend(), [&](const char * Starter)
                { return strcmp(Starter, buf.data()) == 0; }))
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Invalid HTTP request start '%1' - quitting").arg(buf.constData()));
                return false;
            }
        }
    }

    // Read headers
    while (!m_headersComplete && Socket->canReadLine())
    {
        QByteArray line = Socket->readLine().trimmed();
        m_linesRead++;

        // A large header suggests an error
        if (line.size() > 2048)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Unusually long header - quitting");
            return false;
        }

        if (line.isEmpty())
        {
            m_headersComplete = true;
            break;
        }

        if (m_started)
        {
            int index = line.indexOf(":");
            if (index > 0)
            {
                QByteArray key   = line.left(index).trimmed().toLower();
                QByteArray value = line.mid(index + 1).trimmed();
                if (key == "content-length")
                    m_contentLength = value.toLongLong();
                m_headers->insert(key, value);
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Invalid header: '%1'").arg(line.constData()));
                return false;
            }
        }
        else
        {
            m_started = true;
            m_method = line;
        }
    }

    // Need more data...
    if (!m_headersComplete)
        return true;

    // No body?
    if (m_contentLength < 1)
    {
        Ready = true;
        return true;
    }

    // Create a buffer for the content if needed
    if (!m_content)
        m_content = MythHTTPData::Create();

    // Read contents
    while ((Socket->state() == QAbstractSocket::ConnectedState) && Socket->bytesAvailable() &&
           (static_cast<int64_t>(m_content->size()) < m_contentLength))
    {
        int64_t want = m_contentLength - m_content->size();
        int64_t have = Socket->bytesAvailable();
        m_content->append(Socket->read(std::max({want, HTTP_CHUNKSIZE, have})));
    }

    // Need more data...
    if (static_cast<int64_t>(m_content->size()) < m_contentLength)
        return true;

    Ready = true;
    return true;
}
