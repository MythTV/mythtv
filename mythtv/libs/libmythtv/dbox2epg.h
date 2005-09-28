/**
 *  DBox2EPG
 *  Copyright (c) 2005 by Levent Gündogdu (mythtv@feature-it.com)
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DBOX2EPG_H_
#define DBOX2EPG_H_

#include <map>
using namespace std;

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
	void Init(DBox2DBOptions* dbox2_options, int cardid, 
                  DBox2Channel* channel);
	void RequestEPG(const QString& channelNumber);
	void ScheduleRequestEPG(const QString& channelNumber);
	void Shutdown();

    public slots:
        void httpRequestFinished(int requestID, bool error);

    signals:
        void EPGFinished();

    private:
	bool ReadChannels(int cardid);
	void UpdateDataBase(QString* chanID, QDateTime* startTime, QDateTime* endTime, QString* title, QString *description, QString *category);
	QString DBox2EPG::ParseNextLine(QByteArray buffer, int* index, int size);
	QString GetChannelID(const QString& channelnumber) ;
	bool UseOnAirGuide(uint chanid);
	void run();

        QHttp* http;
	DBox2DBOptions* m_dbox2options;
	DBox2Channel* m_dbox2channel;
	int m_cardid;
	int m_channelCount;
	int m_channelIndex;
	map<int, QString> m_channelnumbers;
	bool m_isRunning;
	bool m_inProgress;
	bool m_pendingRequest;
	QString m_requestedChannel;
	QString m_currentEPGRequestChannel;
	int m_currentEPGRequestID;

};


#endif
