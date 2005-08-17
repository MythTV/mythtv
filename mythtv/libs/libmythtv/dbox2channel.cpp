/**
 *  DBox2Channel
 *  Copyright (c) 2005 by Levent Gündogdu
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#include <iostream>
#include <qsqldatabase.h>
#include "mythdbcon.h"
#include "mythcontext.h"
#include "dbox2channel.h"

//#define DBOX2_CHANNEL_DEBUG

DBox2Channel::DBox2Channel(TVRec *parent, dbox2_options_t *dbox2_options, int cardid)
    : ChannelBase(parent)
{
    m_dbox2options = dbox2_options;
    m_cardid = cardid;
    m_dbox2channelcount = 0;
    m_lastChannel = "1";
    m_requestChannel = "";
    m_channelListReady = false;
    capchannels = 1;
    channelnames[0] = "DBOX2";
    // Create EPG
    m_epg = new DBox2EPG();
    m_recorderAlive = false;
    // Create http
    http = new QHttp();
    httpChanger = new QHttp();
    // Connect signals
    connect (http, SIGNAL(done(bool)), this, SLOT(HttpRequestDone(bool)));
    connect (httpChanger, SIGNAL(done(bool)), this, SLOT(HttpChannelChangeDone(bool)));
    connect (m_epg, SIGNAL(EPGFinished()), this, SLOT(EPGFinished()));
    // Load channel names and ids from the dbox
    LoadChannels();
}

DBox2Channel::~DBox2Channel(void)
{   
    // Shutdown EPG
    disconnect (m_epg, SIGNAL(EPGFinished()), this, SLOT(EPGFinished()));
    m_epg->Shutdown();
    delete m_epg;
    // Abort pending channel changes
    httpChanger->abort();
    httpChanger->closeConnection();
    disconnect (httpChanger, SIGNAL(done(bool)), this, SLOT(httpChannelChangeDone(bool)));
    // Abort pending channel list requests
    http->abort();
    http->closeConnection();
    disconnect (http, SIGNAL(done(bool)), this, SLOT(httpRequestDone(bool)));
    delete http;
}

void DBox2Channel::SwitchToLastChannel() 
{
    Log(QString("Switching to last channel %1.").arg(m_lastChannel));
    SetChannelByString(m_lastChannel);
}

bool DBox2Channel::SetChannelByString(const QString &newChan)
{
    // Delay set channel when list has not yet been retrieved
    if (!m_channelListReady)
    {
        Log(QString("Channel list not received yet. "
                    "Will switch to channel %1 later...").arg(newChan));
	m_requestChannel = newChan;
	return true;
    }
    QString chan = newChan;
    if (chan == "")
    {
        Log(QString("Empty channel name has been provided. Getting default name."));
	chan = GetDefaultChannel();
    }
    Log(QString("Changing to %1.").arg(chan));
    if (m_lastChannel != curchannelname)
        m_lastChannel = curchannelname;
    curchannelname = chan;

    // Zap DBox2 to requested channel
    // Find channel name from channel number
    QString channelName = GetChannelNameFromNumber(chan);
    if (channelName == "")
    {
        Log(QString("Changing to %1 failed. Channel not found!").arg(chan));
	QString defaultChannel = GetDefaultChannel();
	if (defaultChannel != chan) 
	{  
	    Log(QString("Trying default channel %1").arg(defaultChannel));
	    return SetChannelByString(defaultChannel);
	}
	return false;
    }

    // Find dbox2 channel id from channel name
    QString channelID = GetChannelID(channelName);
    if (channelID == "")
    {
        Log(QString("Changing to %1 failed. "
                    "DBox2 channel ID for name %2 not found!")
            .arg(chan).arg(channelName));
	QString defaultChannel = GetDefaultChannel();
	if (defaultChannel != chan) 
	{  
	    Log(QString("Trying default channel %1").arg(defaultChannel));
	    return SetChannelByString(defaultChannel);
	}
	return false;
    }

    Log(QString("Channel ID for %1 is %2.").arg(chan).arg(channelID));

    // Request channel change
    ChannelChanging();
    RequestChannelChange(channelID);
    return true;
}

bool DBox2Channel::Open()
{
    Log(QString("Channel instantiated."));
    return true;
}

void DBox2Channel::Close()
{
    Log(QString("Channel destructed."));
}
    
bool DBox2Channel::SwitchToInput(const QString &input, const QString &chan)
{
    currentcapchannel = 0;
    if (channelnames.empty())
        channelnames[currentcapchannel] = input;

    return SetChannelByString(chan);
}

void DBox2Channel::LoadChannels()
{
    Log(QString("Loading channels..."));
    Log(QString("Reading channel list from %1:%2")
                              .arg(m_dbox2options->host)
	                      .arg(m_dbox2options->httpport));

    // Request Channel list via http. Signal will be emmitted when list is ready.
    QHttpRequestHeader header("GET", "/control/channellist");
    header.setValue("Host", m_dbox2options->host);
    http->setHost(m_dbox2options->host, m_dbox2options->httpport);
    http->request(header);
}

void DBox2Channel::HttpRequestDone(bool error) 
{
    if (error) 
    {
        Log(QString("Reading channel list failed!"));
        return;
    }
    
    QString buffer=http->readAll();
    Log(QString("Reading channel list succeeded."));
    m_dbox2channelcount = 0;
    while (true)
    {
        QString line = buffer.section("\n", m_dbox2channelcount, m_dbox2channelcount);
	if (line == "") 
	    break;
	m_dbox2channelids[m_dbox2channelcount] = line.section(" ", 0, 0);
	m_dbox2channelnames[m_dbox2channelcount] = line.section(" ", 1, 5);
#ifdef DBOX2_CHANNEL_DEBUG
	Log(QString("Found Channel %1.").arg(m_dbox2channelnames[m_dbox2channelcount]));
#endif
	m_dbox2channelcount++;
    }
    Log(QString("Read %1 channels.").arg(m_dbox2channelcount));

    // Initialize EPG
    m_epg->Init(m_dbox2options, m_cardid, this);
    
    // Channel list is ready.
    m_channelListReady = true;
    // Change channel if request available
    if (m_requestChannel != "")
    {
	SetChannelByString(m_requestChannel);
	m_requestChannel = "";
    }
}

void DBox2Channel::RequestChannelChange(QString channelID) 
{
    // Prepare request 
    QString requestString;
    requestString = QString("/control/zapto?%1").arg(channelID);

    Log(QString("Changing channel on %1:%2 to channel id %3: %4")
	                       .arg(m_dbox2options->host)
	                       .arg(m_dbox2options->httpport)
	                       .arg(channelID)
	                       .arg(requestString));

    // Request channel change via http. Signal will be emmited when request is done.
    QHttpRequestHeader header("GET", requestString);
    header.setValue("Host", m_dbox2options->host);
    httpChanger->setHost(m_dbox2options->host, m_dbox2options->httpport);
    httpChanger->request(header);
}

void DBox2Channel::HttpChannelChangeDone(bool error) 
{
    if (error) 
    {
        Log(QString("Changing channel failed!"));
        return;
    }
    
    QString buffer=httpChanger->readAll();
    QString line = buffer;
    if (line == "ok")
    {
        Log(QString("Changing channel succeeded..."));
	// Send signal to record that channel has changed.
	ChannelChanged();
	// Request EPG for this channel if recorder is not alive
	if (!m_recorderAlive)
	  m_epg->ScheduleRequestEPG(curchannelname);
	return;
    }

    Log(QString("Changing channel failed: %1.").arg(line));
    return;
}

QString DBox2Channel::GetChannelID(const QString& name)
{
    for (int i = m_dbox2channelcount-1; i >= 0; i--)
        if (m_dbox2channelnames[i].upper() == name.upper())
            return m_dbox2channelids[i];
    return "";
}

QString DBox2Channel::GetChannelNameFromNumber(const QString& channelnumber) 
{
    MSqlQuery query(MSqlQuery::InitCon());   
    query.prepare("SELECT name "
		  "FROM channel,cardinput "
                  "WHERE "
		  "channel.sourceid = cardinput.sourceid AND "
		  "cardinput.cardid = :CARDID AND "
                  "channel.channum = :CHANNUM");
    query.bindValue(":CARDID", m_cardid);
    query.bindValue(":CHANNUM", channelnumber);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toString();
    } 
    return "";
}

QString DBox2Channel::GetChannelNumberFromName(const QString& channelName) 
{
    Log(QString("Getting channel number from channel %1.").arg(channelName));

    MSqlQuery query(MSqlQuery::InitCon());   
    query.prepare("SELECT channum "
		  "FROM channel, cardinput "
                  "WHERE "
		  "channel.sourceid = cardinput.sourceid AND "
		  "cardinput.cardid = :CARDID AND "
                  "channel.name = :NAME");
    query.bindValue(":CARDID", m_cardid);
    query.bindValue(":NAME", channelName);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toString();
    } 
    Log(QString("Channel number from channel %1 is unknown.").arg(channelName));
    return "";
}

QString DBox2Channel::GetDefaultChannel()
{
    MSqlQuery query(MSqlQuery::InitCon());   
    query.prepare("SELECT channum "
		  "FROM channel,cardinput "
                  "WHERE "
		  "channel.sourceid = cardinput.sourceid AND "
		  "cardinput.cardid = :CARDID "
		  "ORDER BY channum limit 1");
    query.bindValue(":CARDID", m_cardid);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        return query.value(0).toString();
    } 
    return "";
}

void DBox2Channel::Log(QString string) 
{
    VERBOSE(VB_IMPORTANT,QString("DBOX#%1: %2").arg(m_cardid).arg(string));
}

void DBox2Channel::RecorderAlive(bool alive)
{
    if (m_recorderAlive == alive)
        return;

    m_recorderAlive = alive;
    if (m_recorderAlive)
        Log("Recorder now online. Deactivating EPG scan");
    else  
    {
	Log("Recorder now offline. Reactivating EPG scan");
	ScanNextEPG();
    }
}

void DBox2Channel::EPGFinished()
{
    // Switch to next channel to retrieve EPG
    if (m_recorderAlive)
        Log("EPG finished. Recorder still online. Deactivating EPG scan");
    else  
    {
	Log("EPG finished. Recorder still offline. Continuing EPG scan");
	ScanNextEPG();
    }
}

void DBox2Channel::ScanNextEPG()
{
    SetChannelByDirection(CHANNEL_DIRECTION_UP);
}
