/** -*- Mode: c++ -*-
 *  CetonRTSP
 *  Copyright (c) 2011 Ronald Frazier
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <QStringList>
#include <QTcpSocket>
#include <QUrl>

// MythTV includes
#include "cetonrtsp.h"
#include "mythlogging.h"


#define LOC QString("CetonRTSP(%1): ").arg(_requestUrl)

QMutex CetonRTSP::_rtspMutex;

CetonRTSP::CetonRTSP(const QString &ip, uint tuner, ushort port) :
    _ip(ip),
    _port(port),
    _sequenceNumber(0),
    _sessionId("0"),
    _responseCode(-1)
{
    _requestUrl = QString("rtsp://%1:%2/cetonmpeg%3")
        .arg(ip).arg(port).arg(tuner);
}

CetonRTSP::CetonRTSP(const QUrl &url) :
    _ip(url.host()),
    _port((url.port() >= 0) ? url.port() : 554),
    _sequenceNumber(0),
    _sessionId("0"),
    _responseCode(-1)
{
    _requestUrl = url.toString();
}

bool CetonRTSP::ProcessRequest(
    const QString &method, const QStringList* headers)
{
    QMutexLocker locker(&_rtspMutex);
    QTcpSocket socket;
    socket.connectToHost(_ip, _port);

    QStringList requestHeaders;
    requestHeaders.append(QString("%1 %2 RTSP/1.0").arg(method, _requestUrl));
    requestHeaders.append(QString("User-Agent: MythTV Ceton Recorder"));
    requestHeaders.append(QString("CSeq: %1").arg(++_sequenceNumber));
    if (_sessionId != "0")
        requestHeaders.append(QString("Session: %1").arg(_sessionId));
    if (headers != NULL)
    {
        for(int i = 0; i < headers->count(); i++)
        {
            QString header = headers->at(i);
            requestHeaders.append(header);
        }
    }
    requestHeaders.append(QString("\r\n"));
    QString request = requestHeaders.join("\r\n");


    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("write: %1").arg(request));
    socket.write(request.toLatin1());

    _responseHeaders.clear();
    _responseContent.clear();

    QRegExp firstLineRegex(
        "^RTSP/1.0 (\\d+) ([^\r\n]+)", Qt::CaseSensitive, QRegExp::RegExp2);
    QRegExp headerRegex(
        "^([^:]+):\\s*([^\\r\\n]+)", Qt::CaseSensitive, QRegExp::RegExp2);
    QRegExp blankLineRegex(
        "^[\\r\\n]*$", Qt::CaseSensitive, QRegExp::RegExp2);

    bool firstLine = true;
    while (true)
    {
        if (!socket.canReadLine())
        {
            bool ready = socket.waitForReadyRead();
            if (!ready)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + "RTSP server did not respond");
                return false;
            }
            continue;
        }

        QString line = socket.readLine();
        LOG(VB_RECORD, LOG_DEBUG, LOC + QString("read: %1").arg(line));

        if (firstLine)
        {
            if (firstLineRegex.indexIn(line) == -1)
            {
                _responseCode = -1;
                _responseMessage =
                    QString("Could not parse first line of response: '%1'")
                    .arg(line);
                return false;
            }

            QStringList parts = firstLineRegex.capturedTexts();
            _responseCode = parts.at(1).toInt();
            _responseMessage = parts.at(2);

            firstLine = false;
            continue;
        }

        if (blankLineRegex.indexIn(line) != -1) break;

        if (headerRegex.indexIn(line) == -1)
        {
            _responseCode = -1;
            _responseMessage = QString("Could not parse response header: '%1'")
                .arg(line);
            return false;
        }
        QStringList parts = headerRegex.capturedTexts();
        _responseHeaders.insert(parts.at(1), parts.at(2));
    }

    QString cSeq = _responseHeaders.value("CSeq");
    if (cSeq != QString("%1").arg(_sequenceNumber))
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("Expected CSeq of %1 but got %2")
            .arg(_sequenceNumber).arg(cSeq));
    }

    _responseContent.clear();
    int contentLength = _responseHeaders.value("Content-Length").toInt();
    if (contentLength > 0)
    {
        _responseContent.resize(contentLength);
        char* data = _responseContent.data();
        int bytesRead = 0;
        while (bytesRead < contentLength)
        {
            if (socket.bytesAvailable() == 0)
                socket.waitForReadyRead();

            int count = socket.read(data+bytesRead, contentLength-bytesRead);
            if (count == -1)
            {
                _responseCode = -1;
                _responseMessage = "Could not read response content";
                return false;
            }
            bytesRead += count;
        }
    }
    return true;
}

bool CetonRTSP::GetOptions(QStringList &options)
{
    if (ProcessRequest("OPTIONS"))
    {
        options = _responseHeaders.value("Public").split(QRegExp(",\\s*"));
        return true;
    }
    return false;
}

bool CetonRTSP::Describe(void)
{
    if (!ProcessRequest("DESCRIBE"))
        return false;

    if (!_responseContent.contains("m=video 0 RTP/AVP 33"))
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "expected content to be type "
            "\"m=video 0 RTP/AVP 33\" but it appears not to be");
        return false;
    }

    return true;
}

bool CetonRTSP::Setup(ushort clientPort1, ushort clientPort2)
{
    LOG(VB_GENERAL, LOG_INFO, QString("CetonRTSP: ") +
        QString("Transport: RTP/AVP;unicast;client_port=%1-%2")
        .arg(clientPort1).arg(clientPort2));

    QStringList extraHeaders;
    extraHeaders.append(
        QString("Transport: RTP/AVP;unicast;client_port=%1-%2")
        .arg(clientPort1).arg(clientPort2));

    if (!ProcessRequest("SETUP", &extraHeaders))
        return false;

    _sessionId = _responseHeaders.value("Session");
    if (_sessionId.size() < 8)
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "session id not found in SETUP response");
        return false;
    }

    return true;
}

bool CetonRTSP::Play(void)
{
    return ProcessRequest("PLAY");
}

bool CetonRTSP::Teardown(void)
{
    bool result = ProcessRequest("TEARDOWN");
    _sessionId = "0";
    return result;
}
