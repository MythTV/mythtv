/** -*- Mode: c++ -*-
 *  CetonRTSP
 *  Copyright (c) 2011 Ronald Frazier
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONRTSP_H
#define CETONRTSP_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QMutex>

class QUrl;

class CetonRTSP
{
  public:
    explicit CetonRTSP(const QString &ip, uint tuner, ushort port);
    explicit CetonRTSP(const QUrl&);

    bool GetOptions(QStringList &options);
    bool Describe(void);
    bool Setup(ushort clientPort1, ushort clientPort2);
    bool Play(void);
    bool Teardown(void);

  protected:
    bool ProcessRequest(
        const QString &method, const QStringList *headers = NULL);

  private:
    QString     _ip;
    ushort      _port;
    uint        _sequenceNumber;
    QString     _sessionId;
    QString     _requestUrl;

    int                     _responseCode;
    QString                 _responseMessage;
    QHash<QString,QString>  _responseHeaders;
    QByteArray              _responseContent;

    static QMutex _rtspMutex;

};

#endif // CETONRTSP_H
