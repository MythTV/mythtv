/** -*- Mode: c++ -*-
 *  CetonRTSP
 *  Copyright (c) 2011 Ronald Frazier
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONRTSP_H
#define CETONRTSP_H

#include <stdint.h>

#include <QObject>
#include <QMap>
#include <QString>
#include <QMutex>
#include <QUrl>
#include <QTimerEvent>

class QTcpSocket;
class QUdpSocket;

typedef QMap<QString, QString> Params;

class CetonRTSP : QObject
{
    Q_OBJECT

  public:
    explicit CetonRTSP(const QString &ip, uint tuner, ushort port);
    explicit CetonRTSP(const QUrl&);
    ~CetonRTSP();

    bool GetOptions(QStringList &options);
    bool Describe(void);
    bool Setup(ushort clientPort1, ushort clientPort2,
               ushort &rtpPort, ushort &rtcpPort, uint32_t &ssrc);
    bool Play(void);
    bool Teardown(void);

    void StartKeepAlive(void);
    void StopKeepAlive(void);

protected:
    bool ProcessRequest(
        const QString &method, const QStringList *headers = NULL,
                        bool use_control = false, bool waitforanswer = true);

  private:
    QStringList splitLines(const QByteArray &lines);
    QString readParamaters(const QString &key, Params &parameters);
    QUrl GetBaseUrl(void);
    void timerEvent(QTimerEvent*);

    QTcpSocket *_socket;
    uint        _sequenceNumber;
    QString     _sessionId;
    QUrl        _requestUrl;
    QUrl        _controlUrl;

    int                     _responseCode;
    QString                 _responseMessage;
    Params                  _responseHeaders;
    QByteArray              _responseContent;
    int                     _timeout;
    int                     _timer;

    static QMutex _rtspMutex;

};

#endif // CETONRTSP_H
