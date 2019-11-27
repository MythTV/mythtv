/** -*- Mode: c++ -*-
 *  CetonRTSP
 *  Copyright (c) 2011 Ronald Frazier
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONRTSP_H
#define CETONRTSP_H

#include <cstdint>

#include <QObject>
#include <QMap>
#include <QString>
#include <QMutex>
#include <QUrl>
#include <QTimerEvent>

class QTcpSocket;
class QUdpSocket;

using Params = QMap<QString, QString>;

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
        const QString &method, const QStringList *headers = nullptr,
                        bool use_control = false, bool waitforanswer = true,
                        const QString &alternative = QString());

  private:
    static QStringList splitLines(const QByteArray &lines);
    QString readParameters(const QString &key, Params &parameters);
    QUrl GetBaseUrl(void);
    void timerEvent(QTimerEvent*) override; // QObject

    QTcpSocket    *m_socket          {nullptr};
    uint           m_sequenceNumber  {0};
    QString        m_sessionId       {"0"};
    QUrl           m_requestUrl;
    QUrl           m_controlUrl;

    int            m_responseCode    {-1};
    QString        m_responseMessage;
    Params         m_responseHeaders;
    QByteArray     m_responseContent;
    int            m_timeout         {60};
    int            m_timer           {0};
    bool           m_canGetParameter {false};

    static QMutex  s_rtspMutex;

};

#endif // CETONRTSP_H
