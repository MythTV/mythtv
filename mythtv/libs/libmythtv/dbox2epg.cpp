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

#define LOC QString("DBOXEPG#%1: ").arg(m_cardid)
DBox2EPG::DBox2EPG()
    : http(new QHttp()),
      m_dbox2options(NULL),
      m_dbox2channel(NULL),
      m_cardid(-1),
      m_channelCount(0),
      m_channelIndex(0),
      m_isRunning(true),
      m_inProgress(false),
      m_pendingRequest(false),
      m_requestedChannel(""),
      m_currentEPGRequestChannel(""),
      m_currentEPGRequestID(-1)
{
    connect(http, SIGNAL(requestFinished    (int,bool)),
            this, SLOT(  httpRequestFinished(int,bool)));
}

DBox2EPG::~DBox2EPG()
{
    TeardownAll();
}

/** \fn DBox2EPG::deleteLater(void)
 *  \brief Safer alternative to just deleting DVBChannel directly.
 */
void DBox2EPG::deleteLater(void)
{
    disconnect(); // disconnect signals we may be sending...
    TeardownAll();
    QObject::deleteLater();
}

void DBox2EPG::TeardownAll(void)
{
    if (http)
    {
        // Abort any pending http operation
        http->abort();
        http->closeConnection();
        // Disconnect from http 
        disconnect(http, SIGNAL(requestFinished    (int,bool)),
                   this, SLOT(  httpRequestFinished(int,bool)));
        // Delete http
        http->deleteLater();
    }
}

void DBox2EPG::Init(DBox2DBOptions* dbox2_options, int cardid,
                    DBox2Channel* channel)
{
    VERBOSE(VB_RECORD, LOC + "Init run");

    m_dbox2options = dbox2_options;
    m_dbox2channel = channel;
    m_cardid       = cardid;

    http->setHost(m_dbox2options->host, m_dbox2options->httpport);

    start();
}

void DBox2EPG::Shutdown()
{
    m_isRunning = false;
}

void DBox2EPG::run() 
{
    VERBOSE(VB_RECORD, LOC + "Starting Thread....");
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

        int chanid = GetChannelID(m_requestedChannel);

        if (chanid < 0)
            continue;

	// Only grab the EPG for this channel if useonairguide is set to 1
	if (UseOnAirGuide((uint)chanid))
        {
            RequestEPG(m_requestedChannel);
            m_pendingRequest = false;
        }
	else
        {
            VERBOSE(VB_RECORD, LOC + QString("EPG disabled for %1.")
                    .arg(m_requestedChannel));
            EPGFinished();
        }
    }
    VERBOSE(VB_RECORD, LOC + "Exiting Thread....");
}


void DBox2EPG::ScheduleRequestEPG(const QString& channelNumber) 
{
    m_requestedChannel = channelNumber;
    m_pendingRequest = true;
}

void DBox2EPG::RequestEPG(const QString& channelNumber)
{
    // Prepare request 
    QString requestString, channelName, dbox2ChannelID;
    channelName    = m_dbox2channel->GetChannelNameFromNumber(channelNumber);
    dbox2ChannelID = m_dbox2channel->GetChannelID(channelName);
    requestString  = QString("/control/epg?id=%1").arg(dbox2ChannelID);

    VERBOSE(VB_RECORD, LOC +
            QString("Requesting EPG for channel %2 (%3): %4%5")
	    .arg(m_cardid)
	    .arg(channelNumber)
	    .arg(channelName)
	    .arg(m_dbox2options->host)
	    .arg(requestString));
    
    // Request EPG via http. Signal will be emmited when request is done.
    QHttpRequestHeader header("GET", requestString);
    header.setValue("Host", m_dbox2options->host);

    m_currentEPGRequestChannel = channelNumber;
    m_currentEPGRequestID      = http->request(header);
}


void DBox2EPG::UpdateDB(uint chanid,
                        const QDateTime &startTime,
                        const QDateTime &endTime,
                        const QString   &title,
                        const QString   &description,
                        const QString   &category)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Delete old info
    query.prepare("DELETE FROM program "
                  "WHERE chanid    = :CHANID AND "
                  "      starttime = :STARTTIME");

    query.bindValue(":CHANID", chanid);
    query.bindValue(":STARTTIME", startTime.toString("yyyyMMddhhmmss"));

    if (!query.exec())
        MythContext::DBError("Deleting old program", query);

    // Inserting new info
    query.prepare("INSERT INTO program "
                  "    (chanid,   starttime,  endtime,      "
                  "     title,    subtitle,   description,  "
                  "     category, airdate,    stars)        "
                  "VALUES "
                  "    (:CHANID,  :STARTTIME, :ENDTIME,     "
                  "     :TITLE,   :SUBTITLE,  :DESCRIPTION, "
                  "     :CATEGORY,:AIRDATE,   :STARS)");

    query.bindValue(":CHANID",      chanid);
    query.bindValue(":STARTTIME",   startTime.toString("yyyyMMddhhmmss"));
    query.bindValue(":ENDTIME",     endTime.toString("yyyyMMddhhmmss"));
    query.bindValue(":TITLE",       title.utf8());
    query.bindValue(":SUBTITLE",    "");
    query.bindValue(":DESCRIPTION", description.utf8());
    query.bindValue(":CATEGORY",    category.utf8());
    query.bindValue(":AIRDATE",     "0");
    query.bindValue(":STARS",       "0");

    if (!query.exec())
        MythContext::DBError("Saving new program", query);
}

/** \fn DBox2EPG::httpRequestFinished(int requestID, bool error)
 *  \brief Reads the results and updates the database accordingly.
 *
 *   This is called by the HTTP object once a request has been finished.
 *
 */

void DBox2EPG::httpRequestFinished(int requestID, bool error)
{
    if (error) 
    {
        VERBOSE(VB_RECORD, LOC + "Reading EPG failed.");
	EPGFinished();
        return;
    }

    if (requestID != m_currentEPGRequestID)
    {
        VERBOSE(VB_RECORD, LOC + "Got EPG for old channel. Ignoring");
	return;
    }

    QByteArray buffer = http->readAll();
    int size   = buffer.size();
    int chanid = GetChannelID(m_currentEPGRequestChannel);

    VERBOSE(VB_RECORD, LOC + "EPG received. " +
            QString("Parsing %2 bytes...").arg(size));

    // Parse 
    int showCount = 0;
    int index     = 0;

    QDateTime startTime;
    QDateTime endTime;
    QString epgID, title, category, desc;

    while (index < size)
    {
        // Next section must not start with an empty line
        QString line = ParseNextLine(buffer, index, size);

	if (line == "") 
	  continue;

        // Parse info
	epgID    = line.section(" ", 0, 0);
	startTime.setTime_t(line.section(" ", 1, 1).toInt());
	endTime  = startTime.addSecs(line.section(" ", 2, 2).toInt());

        title    = ParseNextLine(buffer, index, size);
        category = ParseNextLine(buffer, index, size);
        desc     = ParseNextLine(buffer, index, size);


        // Print debug
	VERBOSE(VB_RECORD, LOC + 
                QString("Found show. Start Time: %1, End Time: %2, "
                        "Title: %3, Description: %4.")
                .arg(m_cardid)
                .arg(startTime.toString()).arg(endTime.toString())
                .arg(title).arg(desc));

	// Update database
	UpdateDB(chanid, startTime, endTime, title, desc, category);

	showCount++;
    }

    VERBOSE(VB_RECORD, LOC + "EPG parsing done. " +
            QString("Got %2 shows for channel %3.")
            .arg(showCount).arg(m_currentEPGRequestChannel));

    EPGFinished();
}

QString DBox2EPG::ParseNextLine(const QByteArray &buf, int &pos, int size)
{
    QString string;
    while (pos < size) 
    {
	char current = buf[pos];
	pos++;
	if (current == '\n')
	    break;
	string += current;
    }
    return string;
}

int DBox2EPG::GetChannelID(const QString& channum) 
{
    MSqlQuery query(MSqlQuery::InitCon());   

    query.prepare(
        "SELECT chanid "
        "FROM channel "
        "WHERE cardid  = :CARDID AND "
        "      channum = :CHANNUM");

    query.bindValue(":CARDID",  m_cardid);
    query.bindValue(":CHANNUM", channum);

    if (query.exec() && query.isActive() && query.next())
        return query.value(0).toInt();

    return -1;
}

/** \fn DBox2EPG::UseOnAirGuide(uint)
 *  \brief Returns use on air guide status of channel
 *  \param chanid Channel ID of channel.
 */
bool DBox2EPG::UseOnAirGuide(uint chanid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT useonairguide "
        "FROM channel "
        "WHERE chanid = :CHANID");

    query.bindValue(":CHANID", chanid);

    if (query.exec() && query.isActive() && query.next())
        return (bool) query.value(0).toInt();

    return false;
}
