/**
 *  DBox2EPG
 *  Copyright (c) 2005 by Levent Gündogdu (mythtv@feature-it.com)
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include <qdatetime.h>
#include "dbox2epg.h"
#include "mythdbcon.h"

#define DEBUG_DBOX2EPG
//#define DEBUG_DBOX2EPG_SHOWS
//#define DEBUG_DBOX2EPG_PARSER

DBox2EPG::DBox2EPG() 
{
    http = new QHttp();
    connect (http, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int,bool)));
    m_isRunning = true;
    m_dbox2channel = NULL;
}

DBox2EPG::~DBox2EPG()
{
    // Abort any pending http operation
    http->abort();
    http->closeConnection();
    // Disconnect from http 
    disconnect (http, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int,bool)));
    // Delete http
    delete http;
}

void DBox2EPG::Init(dbox2_options_t* dbox2_options, int cardid, DBox2Channel* channel)
{
    m_dbox2options = dbox2_options;
    http->setHost(m_dbox2options->host, m_dbox2options->httpport);
    m_dbox2channel = channel;
    m_cardid = cardid;
#ifdef DEBUG_DBOX2EPG
    VERBOSE(VB_IMPORTANT, QString("DBOXEPG#%1: Initing.").arg(m_cardid));
#endif
    start();
}

void DBox2EPG::Shutdown()
{
    m_isRunning = false;
}

void DBox2EPG::run() 
{
#ifdef DEBUG_DBOX2EPG
    VERBOSE(VB_IMPORTANT, QString("DBOXEPG#%1: Starting Thread....").arg(m_cardid));
#endif
    long waitTime = 15 * 1000 * 1000;

    // Process all channel ids and retrieve EPG
    while (m_isRunning) 
    {
        usleep(1000);

        // If operation is in progress, do not request epg
        if (!m_pendingRequest)
	    continue;

        // Wait before processing
	usleep(waitTime);

	RequestEPG(m_requestedChannel);
	m_pendingRequest = false;
    }
#ifdef DEBUG_DBOX2EPG
    VERBOSE(VB_IMPORTANT, QString("DBOXEPG#%1: Exiting Thread....").arg(m_cardid));
#endif
}


void DBox2EPG::ScheduleRequestEPG(const QString& channelNumber) 
{
    m_requestedChannel = channelNumber;
    m_pendingRequest = true;
}

void DBox2EPG::RequestEPG(const QString& channelNumber)
{
    // Prepare request 
    QString requestString;
    QString channelName = m_dbox2channel->GetChannelNameFromNumber(channelNumber);
    QString dbox2ChannelID = m_dbox2channel->GetChannelID(channelName);
    requestString = QString("/control/epg?id=%1").arg(dbox2ChannelID);

    VERBOSE(VB_IMPORTANT, QString("DBOXEPG#%1: Requesting EPG for channel %2 (%3): %4%5")
	    .arg(m_cardid)
	    .arg(channelNumber)
	    .arg(channelName)
	    .arg(m_dbox2options->host)
	    .arg(requestString));
    
    // Request EPG via http. Signal will be emmited when request is done.
    QHttpRequestHeader header("GET", requestString);
    header.setValue("Host", m_dbox2options->host);
    m_currentEPGRequestChannel = channelNumber;
    m_currentEPGRequestID = http->request(header);
}


void DBox2EPG::UpdateDataBase(QString* chanID, QDateTime* startTime, QDateTime* endTime, QString* title, QString *description, QString *category)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // This used to be REPLACE INTO...
    // primary key of table program is chanid,starttime
    query.prepare("DELETE FROM program"
                  " WHERE chanid = :CHANID"
                  " AND starttime = :STARTTIME ;");
    query.bindValue(":CHANID", chanID->toInt());
    query.bindValue(":STARTTIME", startTime->toString("yyyyMMddhhmmss"));
    if (!query.exec())
        MythContext::DBError("Saving program", 
                             query);

    query.prepare("INSERT INTO program (chanid,starttime,endtime,"
                  " title,subtitle,description,category,airdate,"
                  " stars) VALUES (:CHANID,:STARTTIME,:ENDTIME,:TITLE,"
                  " :SUBTITLE,:DESCRIPTION,:CATEGORY,:AIRDATE,:STARS);");
    query.bindValue(":CHANID", chanID->toInt());
    query.bindValue(":STARTTIME", startTime->toString("yyyyMMddhhmmss"));
    query.bindValue(":ENDTIME", endTime->toString("yyyyMMddhhmmss"));
    query.bindValue(":TITLE", title->utf8());
    query.bindValue(":SUBTITLE", "");
    query.bindValue(":DESCRIPTION", description->utf8());
    query.bindValue(":CATEGORY", category->utf8());
    query.bindValue(":AIRDATE", "0");
    query.bindValue(":STARS", "0");

    if (!query.exec())
        MythContext::DBError("Saving program", 
                             query);
}

/**
 *
 *  Reads the results and updates the database accordingly.
 *  Will be called by the HTTP Object once a request has been finished. 
 *
 */

void DBox2EPG::httpRequestFinished(int requestID, bool error)
{
    if (error) 
    {
        VERBOSE(VB_IMPORTANT, QString("DBOXEPG#%1: Reading EPG failed.").arg(m_cardid));
	EPGFinished();
        return;
    }

    if (requestID != m_currentEPGRequestID)
    {
#ifdef DEBUG_DBOX2EPG
        VERBOSE(VB_IMPORTANT, QString("DBOXEPG#%1: Got EPG for old channel. Ignoring").arg(m_cardid));
#endif
	return;
    }

    QByteArray buffer=http->readAll();
#ifdef DEBUG_DBOX2EPG
    VERBOSE(VB_GENERAL,QString("DBOXEPG#%1: EPG received. Parsing %2 bytes...").arg(m_cardid).arg(buffer.size()));
#endif

    QString channelID = GetChannelID(m_currentEPGRequestChannel);

    // Parse 
    int showCount = 0;
    int index = 0;
    int size = buffer.size();
    while (index < size)
    {
        // Next section must not start with an empty line
        QString line = ParseNextLine(buffer, &index, size);
	if (line == "") 
	  continue;
	QString epgID = line.section(" ", 0, 0);
	QDateTime startTime;
	startTime.setTime_t(line.section(" ", 1, 1).toInt());
	QDateTime endTime;
	endTime = startTime.addSecs(line.section(" ", 2, 2).toInt());
        QString title = ParseNextLine(buffer, &index, size);
        QString category = ParseNextLine(buffer, &index, size);
        QString description = ParseNextLine(buffer, &index, size);
	// Update database
#ifdef DEBUG_DBOX2EPG_SHOWS
	VERBOSE(VB_GENERAL,QString("DBOXEPG#%1: Found show. Start Time: %1, End Time: %2, Title: %3, Description: %4.")
  		.arg(m_cardid)
		.arg(startTime.toString())
		.arg(endTime.toString())
 		.arg(title)
		.arg(description));
#endif
	UpdateDataBase(&channelID, &startTime, &endTime, &title, &description, &category);
	showCount++;

	// Read until empty line (next empty line indicates end of section).
	while (index < size && (line = ParseNextLine(buffer, &index, size)) != "");
    }

    VERBOSE(VB_GENERAL,QString("DBOXEPG#%1: EPG parsing done. Got %2 shows for channel %3.").arg(m_cardid).arg(showCount).arg(m_currentEPGRequestChannel));
    EPGFinished();
}

QString DBox2EPG::ParseNextLine(QByteArray buffer, int* index, int size) 
{
#ifdef DEBUG_DBOX2EPG_PARSER
    VERBOSE(VB_GENERAL,QString("DBOXEPG#%1: Parsing buffer at %2.").arg(m_cardid).arg(*index));
#endif
    QString string;
    while (*index < size) 
    {
	char current = buffer[*index];
	*index = *index + 1;
	if (current == '\n')
	    break;
	string += current;
    }
#ifdef DEBUG_DBOX2EPG_PARSER
    VERBOSE(VB_GENERAL,QString("DBOXEPG#%1: Returning %2.").arg(m_cardid).arg(string));
#endif
    return string;
}

QString DBox2EPG::GetChannelID(const QString& channelnumber) 
{
    MSqlQuery query(MSqlQuery::InitCon());   
    query.prepare("SELECT chanid "
		  "FROM channel "
                  "WHERE "
                  "channel.channum = :CHANNUM");
    query.bindValue(":CHANNUM", channelnumber);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toString();
    } 
    return "";
}

