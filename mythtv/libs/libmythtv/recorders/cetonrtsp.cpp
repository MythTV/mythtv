/** -*- Mode: c++ -*-
 *  CetonRTSP
 *  Copyright (c) 2011 Ronald Frazier
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <QStringList>
#include <QTcpSocket>
#include <QUrl>
#include <QVector>

// MythTV includes
#include "cetonrtsp.h"
#include "mythlogging.h"
#include "mythsocket.h"


#define LOC QString("CetonRTSP(%1): ").arg(_requestUrl.toString())

QMutex CetonRTSP::_rtspMutex;

CetonRTSP::CetonRTSP(const QString &ip, uint tuner, ushort port) :
    _socket(NULL),
    _sequenceNumber(0),
    _sessionId("0"),
    _responseCode(-1),
    _timeout(60),
    _timer(0),
    _canGetParameter(false)
{
    _requestUrl.setHost(ip);
    _requestUrl.setPort(port);
    _requestUrl.setScheme("rtsp");
    _requestUrl.setPath(QString("cetonmpeg%1").arg(tuner));
}

CetonRTSP::CetonRTSP(const QUrl &url) :
    _socket(NULL),
    _sequenceNumber(0),
    _sessionId("0"),
    _requestUrl(url),
    _responseCode(-1),
    _timeout(60),
    _timer(0),
    _canGetParameter(false)
{
    if (url.port() < 0)
    {
        // default rtsp port
        _requestUrl.setPort(554);
    }
}

CetonRTSP::~CetonRTSP()
{
    StopKeepAlive();
}

bool CetonRTSP::ProcessRequest(
    const QString &method, const QStringList* headers,
    bool use_control, bool waitforanswer, const QString &alternative)
{
    QMutexLocker locker(&_rtspMutex);

    _responseHeaders.clear();
    _responseContent.clear();

    // Create socket if socket object has never been created or in non-connected state
    if (!_socket || _socket->state() != QAbstractSocket::ConnectedState)
    {
        if (!_socket)
        {
            _socket = new QTcpSocket();
        }
        else
        {
            _socket->close();
        }
        _socket->connectToHost(_requestUrl.host(), _requestUrl.port(),
                               QAbstractSocket::ReadWrite);
        bool ok = _socket->waitForConnected();

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Could not connect to server %1:%2")
                .arg(_requestUrl.host()).arg(_requestUrl.port()));
            delete _socket;
            _socket = NULL;
            return false;
        }
    }
    else
    {
        // empty socket's waiting data just in case
        _socket->waitForReadyRead(30);
        do
        {
            QVector<char> trash;
            uint avail = _socket->bytesAvailable();
            trash.resize(std::max((uint)trash.size(), avail));
            _socket->read(trash.data(), avail);
            _socket->waitForReadyRead(30);
        }
        while (_socket->bytesAvailable() > 0);
    }

    QStringList requestHeaders;
    requestHeaders.append(QString("%1 %2 RTSP/1.0")
        .arg(method)
        .arg(alternative.size() ? alternative :
             (use_control ? _controlUrl.toString() : _requestUrl.toString())));
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
    _socket->write(request.toLatin1());

    _responseHeaders.clear();
    _responseContent.clear();

    if (!waitforanswer)
        return true;

    QRegExp firstLineRegex(
        "^RTSP/1.0 (\\d+) ([^\r\n]+)", Qt::CaseSensitive, QRegExp::RegExp2);
    QRegExp headerRegex(
        "^([^:]+):\\s*([^\\r\\n]+)", Qt::CaseSensitive, QRegExp::RegExp2);
    QRegExp blankLineRegex(
        "^[\\r\\n]*$", Qt::CaseSensitive, QRegExp::RegExp2);

    bool firstLine = true;
    while (true)
    {
        if (!_socket->canReadLine())
        {
            bool ready = _socket->waitForReadyRead(30 * 1000);
            if (!ready)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + "RTSP server did not respond after 30s");
                return false;
            }
            continue;
        }

        QString line = _socket->readLine();
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

            if (_responseCode != 200)
            {
                _responseMessage =
                    QString("Server couldn't process the request: '%1'")
                    .arg(_responseMessage);
                return false;
            }
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

    QString cSeq;

    if (_responseHeaders.contains("CSeq"))
    {
        cSeq = _responseHeaders["CSeq"];
    }
    else
    {
        // Handle broken implementation, such as VLC
        // doesn't respect the case of "CSeq", so find it regardless of the spelling
        foreach (QString key, _responseHeaders.keys())
        {
            if (key.compare("CSeq", Qt::CaseInsensitive) == 0)
            {
                cSeq = _responseHeaders.value(key);
                break;
            }
        }
    }
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
            if (_socket->bytesAvailable() == 0)
                _socket->waitForReadyRead();

            int count = _socket->read(data+bytesRead, contentLength-bytesRead);
            if (count == -1)
            {
                _responseCode = -1;
                _responseMessage = "Could not read response content";
                return false;
            }
            bytesRead += count;
        }
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("received: %1").arg(_responseContent.constData()));
    }
    return true;
}

bool CetonRTSP::GetOptions(QStringList &options)
{
    if (ProcessRequest("OPTIONS"))
    {
        options = _responseHeaders.value("Public").split(QRegExp(",\\s*"));
        _canGetParameter = options.contains("GET_PARAMETER");

        return true;
    }
    return false;
}

/**
 * splitLines. prepare SDP content for easy read
 */
QStringList CetonRTSP::splitLines(const QByteArray &lines)
{
    QStringList list;
    QTextStream stream(lines);
    QString line;

    do
    {
        line = stream.readLine();
        if (!line.isNull())
        {
            list.append(line);
        }
    }
    while (!line.isNull());

    return list;
}

/**
 * readParameters. Scan a line like: Session: 1234556;destination=xx;client_port
 * and return the first entry and fill the arguments in the provided Params
 */
QString CetonRTSP::readParameters(const QString &key, Params &parameters)
{
    QString val;

    if (!_responseHeaders.contains(key))
    {
        return val;
    }

    QStringList header = _responseHeaders.value(key).split(";");

    for (int i = 0; i < header.size(); i++)
    {
        QString entry = header[i].trimmed();

        if (i ==0)
        {
            val = entry;
            continue;
        }
        QStringList args = entry.split("=");

        parameters.insert(args[0].trimmed(),
                          args.size() > 1 ? args[1].trimmed() : QString());
    }
    return val;
}

/**
 * Return the base URL for the last DESCRIBE answer
 */
QUrl CetonRTSP::GetBaseUrl(void)
{
    if (_responseHeaders.contains("Content-Base"))
    {
        return _responseHeaders["Content-Base"];
    }
    if (_responseHeaders.contains("Content-Location"))
    {
        return _responseHeaders["Content-Location"];
    }
    return _requestUrl;
}

bool CetonRTSP::Describe(void)
{
    QStringList headers;

    headers.append("Accept: application/sdp");

    if (!ProcessRequest("DESCRIBE", &headers))
        return false;

    // find control url
    QStringList lines = splitLines(_responseContent);
    bool found = false;
    QUrl base = _controlUrl = GetBaseUrl();

    foreach (QString line, lines)
    {
        if (line.startsWith("m="))
        {
            if (found)
            {
                // another new stream, no need to parse further
                break;
            }
            if (!line.startsWith("m=video"))
            {
                // not a video stream
                continue;
            }
            QStringList args = line.split(" ");
            if (args[2] == "RTP/AVP" && args[3] == "33")
            {
                found = true;
            }
            continue;
        }
        if (line.startsWith("c="))
        {
            // TODO, connection parameter
            // assume we will always get a control entry
            continue;
        }
        if (line.startsWith("a=control:"))
        {
            // Per RFC: a=control:rtsp://example.com/foo
            // This attribute may contain either relative and absolute URLs,
            // following the rules and conventions set out in RFC 1808 [25].
            QString url = line.mid(10).trimmed();
            _controlUrl = url;
            if (url == "*")
            {
                _controlUrl = base;
            }
            else if (_controlUrl.isRelative())
            {
                _controlUrl = base.resolved(_controlUrl);
            }
            continue;
        }
    }

    if (!found)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + "expected content to be type "
            "\"m=video 0 RTP/AVP 33\" but it appears not to be");
        _controlUrl = QUrl();
        return false;
    }

    return true;
}

bool CetonRTSP::Setup(ushort clientPort1, ushort clientPort2,
                      ushort &rtpPort, ushort &rtcpPort,
                      uint32_t &ssrc)
{
    LOG(VB_GENERAL, LOG_INFO, QString("CetonRTSP: ") +
        QString("Transport: RTP/AVP;unicast;client_port=%1-%2")
        .arg(clientPort1).arg(clientPort2));

    QStringList extraHeaders;
    extraHeaders.append(
        QString("Transport: RTP/AVP;unicast;client_port=%1-%2")
        .arg(clientPort1).arg(clientPort2));

    if (!ProcessRequest("SETUP", &extraHeaders, true))
        return false;

    Params params;
    QString session = readParameters("Session", params);

    if (session.isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            "session id not found in SETUP response");
        return false;
    }
    if (session.size() < 8)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            "invalid session id received");
    }
    _sessionId = session;

    if (params.contains("timeout"))
    {
        _timeout = params["timeout"].toInt();
    }

    QString transport = readParameters("Transport", params);
    if (params.contains("ssrc"))
    {
        bool ok;
        ssrc = params["ssrc"].toUInt(&ok, 16);
    }
    if (params.contains("server_port"))
    {
        QString line = params["server_port"];
        QStringList val = line.split("-");

        rtpPort = val[0].toInt();
        rtcpPort = val.size() > 1 ? val[1].toInt() : 0;
    }

    return true;
}

bool CetonRTSP::Play(void)
{
    bool result = ProcessRequest("PLAY");

    StartKeepAlive();
    return result;
}

bool CetonRTSP::Teardown(void)
{
    StopKeepAlive();

    bool result = ProcessRequest("TEARDOWN");

    QMutexLocker locker(&_rtspMutex);

    delete _socket;
    _socket = NULL;

    _sessionId = "0";
    return result;
}

void CetonRTSP::StartKeepAlive()
{
    if (_timer)
        return;
    int timeout = std::max(_timeout - 5, 5);
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("Start KeepAlive, every %1s").arg(timeout));
    _timer = startTimer(timeout * 1000);
}

void CetonRTSP::StopKeepAlive()
{
    if (_timer)
    {
        killTimer(_timer);
        LOG(VB_RECORD, LOG_DEBUG, LOC + "Stop KeepAlive");
    }
    _timer = 0;
}

void CetonRTSP::timerEvent(QTimerEvent*)
{
    QStringList dummy;

    LOG(VB_RECORD, LOG_DEBUG, LOC + "Sending KeepAlive");
    if (_canGetParameter)
    {
        ProcessRequest("GET_PARAMETER", NULL, false, false);
    }
    else
    {
        ProcessRequest("OPTIONS", NULL, false, false, "*");
    }
}
