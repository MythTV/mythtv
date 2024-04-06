/** -*- Mode: c++ -*- */

// C++ includes
#include <chrono>

// Qt includes
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QUrl>
#include <QVector>

// MythTV includes
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsocket.h"

#include "rtp/rtcpdatapacket.h"
#include "rtp/rtppacketbuffer.h"
#include "rtp/rtptsdatapacket.h"
#include "rtp/udppacketbuffer.h"
#include "satiprtcppacket.h"
#include "satiprtsp.h"
#include "satipstreamhandler.h"

#define LOC  QString("SatIPRTSP[%1]: ").arg(m_inputId)
#define LOC2 QString("SatIPRTSP[%1](%2): ").arg(m_inputId).arg(m_requestUrl.toString())

QMutex SatIPRTSP::s_rtspMutex;


SatIPRTSP::SatIPRTSP(int inputId)
    : m_inputId(inputId)
{
    // Call across threads
    connect(this, &SatIPRTSP::StartKeepAlive, this, &SatIPRTSP::StartKeepAliveRequested);
    connect(this, &SatIPRTSP::StopKeepAlive, this, &SatIPRTSP::StopKeepAliveRequested);
}

SatIPRTSP::~SatIPRTSP()
{
    emit StopKeepAlive();
}

bool SatIPRTSP::sendMessage(const QString& msg, QStringList *additionalheaders)
{
    QMutexLocker locker(&s_rtspMutex);

    QTcpSocket ctrl_socket;
    ctrl_socket.connectToHost(m_requestUrl.host(), m_requestUrl.port());

    bool ok = ctrl_socket.waitForConnected(10 * 1000);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not connect to server %1:%2")
            .arg(m_requestUrl.host()).arg(m_requestUrl.port()));
        return false;
    }

    QStringList requestHeaders;
    requestHeaders.append(QString("%1 %2 RTSP/1.0").arg(msg, m_requestUrl.toString()));
    requestHeaders.append(QString("User-Agent: MythTV Sat>IP client"));
    requestHeaders.append(QString("CSeq: %1").arg(++m_cseq));

    if (m_sessionid.length() > 0)
    {
        requestHeaders.append(QString("Session: %1").arg(m_sessionid));
    }

    if (additionalheaders != nullptr)
    {
        for (auto& adhdr : *additionalheaders)
        {
            requestHeaders.append(adhdr);
        }
    }

    requestHeaders.append("\r\n");

    for (const auto& requestLine : requestHeaders)
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC + "sendMessage " +
            QString("write: %1").arg(requestLine.simplified()));
    }

    QString request = requestHeaders.join("\r\n");
    ctrl_socket.write(request.toLatin1());

    m_responseHeaders.clear();

    static const QRegularExpression kFirstLineRE { "^RTSP/1.0 (\\d+) ([^\r\n]+)" };
    static const QRegularExpression kHeaderRE    { R"(^([^:]+):\s*([^\r\n]+))"   };
    static const QRegularExpression kBlankLineRE { R"(^[\r\n]*$)"                };

    bool firstLine = true;
    while (true)
    {
        if (!ctrl_socket.canReadLine())
        {
            bool ready = ctrl_socket.waitForReadyRead(10 * 1000);
            if (!ready)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + "RTSP server did not respond after 10s");
                return false;
            }
            continue;
        }

        QString line = ctrl_socket.readLine();
        LOG(VB_RECORD, LOG_DEBUG, LOC + "sendMessage " +
            QString("read: %1").arg(line.simplified()));

        QRegularExpressionMatch match;
        if (firstLine)
        {
            match = kFirstLineRE.match(line);
            if (!match.hasMatch())
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    QString("Could not parse first line of response: '%1'")
                        .arg(line.simplified()));
                return false;
            }

            QStringList parts = match.capturedTexts();
            int responseCode = parts.at(1).toInt();
            const QString& responseMsg = parts.at(2);

            LOG(VB_RECORD, LOG_DEBUG, LOC + QString("response code:%1 message:%2")
                .arg(responseCode).arg(responseMsg));

            if (responseCode != 200)
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    QString("Server couldn't process the request: '%1'")
                        .arg(responseMsg));
                return false;
            }
            firstLine = false;
            continue;
        }

        match = kBlankLineRE.match(line);
        if (match.hasMatch()) break;

        match = kHeaderRE.match(line);
        if (!match.hasMatch())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Could not parse response header: '%1'")
                    .arg(line.simplified()));
            return false;
        }
        QStringList parts = match.capturedTexts();
        m_responseHeaders.insert(parts.at(1).toUpper(), parts.at(2));
    }

    QString cSeq;

    if (m_responseHeaders.contains("CSEQ"))
    {
        cSeq = m_responseHeaders["CSEQ"];
    }

    if (cSeq != QString("%1").arg(m_cseq))
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("Expected CSeq of %1 but got %2").arg(m_cseq).arg(cSeq));
    }

    ctrl_socket.disconnectFromHost();
    if (ctrl_socket.state() != QAbstractSocket::UnconnectedState)
    {
        ctrl_socket.waitForDisconnected();
    }

    return true;
}

bool SatIPRTSP::Setup(const QUrl& url, ushort clientPort1, ushort clientPort2)
{
    m_requestUrl = url;
    LOG(VB_RECORD, LOG_DEBUG, LOC2 + QString("SETUP"));

    if (m_requestUrl.port() != 554)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("Port %1 is used but SatIP specifies RTSP port 554").arg(m_requestUrl.port()));
    }

    if (m_requestUrl.port() < 1)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            QString("Invalid port %1 using 554 instead").arg(m_requestUrl.port()));
        m_requestUrl.setPort(554);
    }


    QStringList headers;
    headers.append(
        QString("Transport: RTP/AVP;unicast;client_port=%1-%2")
            .arg(clientPort1).arg(clientPort2));

    if (!sendMessage("SETUP", &headers))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to send SETUP message");
        return false;
    }

    if (m_responseHeaders.contains("COM.SES.STREAMID"))
    {
        m_streamid = m_responseHeaders["COM.SES.STREAMID"];
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SETUP response did not contain the com.ses.streamID field");
        return false;
    }

    if (m_responseHeaders.contains("SESSION"))
    {
        static const QRegularExpression sessionTimeoutRegex {
            "^([^\\r\\n]+);timeout=([0-9]+)?", QRegularExpression::CaseInsensitiveOption };
        auto match = sessionTimeoutRegex.match(m_responseHeaders["SESSION"]);
        if (!match.hasMatch())
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("Failed to extract session id from session header ('%1')")
                    .arg(m_responseHeaders["Session"]));
        }

        m_sessionid = match.captured(1);
        m_timeout = match.capturedLength(2) > 0
            ? std::chrono::seconds(match.capturedView(2).toInt() / 2)
            : 30s;

        LOG(VB_RECORD, LOG_INFO, LOC + QString("Sat>IP protocol timeout:%1 s")
            .arg(m_timeout.count()));
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "SETUP response did not contain the Session field");
        return false;
    }

    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Setup completed, sessionID = %1, streamID = %2, timeout = %3s")
            .arg(m_sessionid, m_streamid)
            .arg(duration_cast<std::chrono::seconds>(m_timeout).count()));

    return true;
}

bool SatIPRTSP::Play(const QString &pids_str)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "Play(pids_str) " + pids_str);

    m_requestUrl.setQuery(pids_str);
    m_requestUrl.setPath(QString("/stream=%1").arg(m_streamid));

    if (!sendMessage("PLAY"))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Failed to send PLAY message");
        return false;
    }

    emit StartKeepAlive();

    return true;
}

bool SatIPRTSP::Teardown(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC2 + "TEARDOWN");
    emit StopKeepAlive();

    m_requestUrl.setQuery(QString());
    m_requestUrl.setPath(QString("/stream=%1").arg(m_streamid));

    bool result = sendMessage("TEARDOWN");

    if (!result)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "Teardown failed");
    }

    QMutexLocker locker(&s_rtspMutex);

    m_sessionid.clear();
    m_streamid.clear();

    return result;
}

void SatIPRTSP::StartKeepAliveRequested()
{
    if (m_timer)
        return;
    auto timeout = std::max(m_timeout - 5s, 5s);
    m_timer = startTimer(timeout);
    LOG(VB_RECORD, LOG_INFO, LOC + QString("StartKeepAliveRequested(%1) m_timer:%2")
        .arg(timeout.count()).arg(m_timer));
}

void SatIPRTSP::StopKeepAliveRequested()
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("StopKeepAliveRequested() m_timer:%1").arg(m_timer));
    if (m_timer)
    {
        killTimer(m_timer);
    }
    m_timer = 0;
}

void SatIPRTSP::timerEvent(QTimerEvent* timerEvent)
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("Sending KeepAlive timer %1").arg(timerEvent->timerId()));

    m_requestUrl.setPath("/");
    m_requestUrl.setQuery(QString());

    sendMessage("OPTIONS");
}
