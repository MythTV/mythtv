/**
 *  DBox2EPG
 *  Copyright (c) 2005 by Levent Gündogdu (mythtv@feature-it.com)
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DBOX2EPG_H_
#define DBOX2EPG_H_

#include <qmap.h>
#include <qhttp.h>
#include <qobject.h>
#include <qthread.h>

#include "mythcontext.h"
#include "tv_rec.h"
#include "dbox2channel.h"

class DBox2Channel;

class DBox2EPG : public QObject, public QThread
{
    Q_OBJECT
  public:
    DBox2EPG();
    ~DBox2EPG();
    void TeardownAll(void);

    void Init(DBox2DBOptions* dbox2_options, int cardid, 
              DBox2Channel* channel);
    void RequestEPG(const QString& channelNumber);
    void ScheduleRequestEPG(const QString& channelNumber);
    void Shutdown();

  public slots:
    void httpRequestFinished(int requestID, bool error);
    void deleteLater(void);

  signals:
    void EPGFinished();

  private:
    void UpdateDB(uint chanid,
                  const QDateTime &startTime,
                  const QDateTime &endTime,
                  const QString &title,
                  const QString &description,
                  const QString &category);

    static QString ParseNextLine(
        const QByteArray &buffer, int &pos, int size);
    static bool UseOnAirGuide(uint chanid);

    int  GetChannelID(const QString& channelnumber);
    void run();

    QHttp             *http;
    DBox2DBOptions    *m_dbox2options;
    DBox2Channel      *m_dbox2channel;
    QMap<int,QString>  m_channelnumbers;

    int     m_cardid;
    int     m_channelCount;
    int     m_channelIndex;
    bool    m_isRunning;
    bool    m_inProgress;
    bool    m_pendingRequest;
    QString m_requestedChannel;

    QString m_currentEPGRequestChannel;
    int     m_currentEPGRequestID;
};


#endif
