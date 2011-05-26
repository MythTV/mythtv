/** -*- Mode: c++ -*-
 *  IPTVFeederRtsp
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */
#include <algorithm>

#include "iptvfeederrtsp.h"

// Live555 headers
#include <RTSPClient.hh>
#include <BasicUsageEnvironment.hh>
#include <MediaSession.hh>

// MythTV headers
#include "iptvmediasink.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "tspacket.h"

#define LOC QString("IPTVFeedRTSP:")
#define LOC_ERR QString("IPTVFeedRTSP, Error:")

/** \class RTSPData
 *  \brief Helper class use for static Callback handler
 */
class RTSPData
{
  public:
    RTSPData(MediaSubsession *pMediaSubSession) :
        mediaSubSession(pMediaSubSession)
    {
    }

    void SubsessionAfterPlayingCB(void);
    void SubsessionByeHandlerCB(void);

  private:
    MediaSubsession *mediaSubSession;
};

void RTSPData::SubsessionAfterPlayingCB(void)
{
    MediaSubsession *subsession = mediaSubSession;
    Medium::close(subsession->sink);
    subsession->sink = NULL;
}

static void sub_after_playing_cb(void *clientData)
{
    ((RTSPData*)clientData)->SubsessionAfterPlayingCB();
}

void RTSPData::SubsessionByeHandlerCB(void)
{
    SubsessionAfterPlayingCB();
}

static void sub_bye_handler_cb(void *clientData)
{
    ((RTSPData*)clientData)->SubsessionByeHandlerCB();
}

//////////////////////////////////////////////////////////////////////////////

IPTVFeederRTSP::IPTVFeederRTSP() :
    _rtsp_client(NULL),
    _session(NULL)
{
    VERBOSE(VB_RECORD, LOC + "ctor -- success");
}

IPTVFeederRTSP::~IPTVFeederRTSP()
{
    VERBOSE(VB_RECORD, LOC + "dtor -- begin");
    Close();
    VERBOSE(VB_RECORD, LOC + "dtor -- end");
}

bool IPTVFeederRTSP::IsRTSP(const QString &url)
{
    return url.startsWith("rtsp://", Qt::CaseInsensitive);
}

bool IPTVFeederRTSP::Open(const QString &url)
{
    VERBOSE(VB_RECORD, LOC + QString("Open(%1) -- begin").arg(url));

    QMutexLocker locker(&_lock);

    if (_rtsp_client)
    {
        VERBOSE(VB_RECORD, LOC + "Open() -- end 1");

        return true;
    }

    // Begin by setting up our usage environment:
    if (!InitEnv())
        return false;

    // Create our client object:
    _rtsp_client = RTSPClient::createNew(*_live_env, 0, "myRTSP", 0);
    if (!_rtsp_client)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to create RTSP client: %1")
                .arg(_live_env->getResultMsg()));
        FreeEnv();
        return false;
    }

    // Setup URL for the current session
    char *sdpDescription = _rtsp_client->describeURL(
        url.toLatin1().constData());

    _rtsp_client->describeStatus();

    if (!sdpDescription)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString(
                    "Failed to get a SDP "
                    "description from URL: %1 %2")
                .arg(url).arg(_live_env->getResultMsg()));
        return false;
    }

    // Create a media session object from this SDP description:
    _session = MediaSession::createNew(*_live_env, sdpDescription);

    delete[] sdpDescription;

    if (!_session)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Failed to create MediaSession: %1")
                .arg(_live_env->getResultMsg()));
        return false;
    }
    else if (!_session->hasSubsessions())
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "This session has no media subsessions");
        Close();
        return false;
    }

    // Then, setup the "RTPSource"s for the session:
    MediaSubsessionIterator iter(*_session);
    MediaSubsession *subsession;
    bool madeProgress = false;

    while ((subsession = iter.next())) /* <- extra braces for pedantic gcc */
    {
        if (!subsession->initiate(-1))
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("Can't create receiver for: "
                            "%1 / %2 subsession: %3")
                    .arg(subsession->mediumName())
                    .arg(subsession->codecName())
                    .arg(_live_env->getResultMsg()));
        }
        else
        {
            madeProgress = true;

            if (subsession->rtpSource() != NULL)
            {
                unsigned const thresh = 1000000; // 1 second
                subsession->rtpSource()->
                    setPacketReorderingThresholdTime(thresh);
            }
        }
    }

    if (!madeProgress)
        return false;

    // Perform additional 'setup' on each subsession, before playing them:
    madeProgress = false;
    iter.reset();
    while ((subsession = iter.next()) != NULL)
    {
        if (subsession->clientPortNum() == 0)
            continue; // port # was not set

        if (_rtsp_client->setupMediaSubsession(*subsession, false, false))
        {
            madeProgress = true;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("Failed to setup: %1 %2 : %3")
                    .arg(subsession->mediumName())
                    .arg(subsession->codecName())
                    .arg(_live_env->getResultMsg()));
        }
    }

    if (!madeProgress)
        return false;

    // Create and start "FileSink"s for each subsession:
    // FileSink while receive Mpeg2 TS Data & will feed them to mythtv
    madeProgress = false;
    iter.reset();

    while ((subsession = iter.next())) /* <- extra braces for pedantic gcc */
    {
        if (!subsession->readSource())
            continue; // was not initiated

        IPTVMediaSink *iptvMediaSink = IPTVMediaSink::CreateNew(
            *_live_env, TSPacket::kSize * 128*1024);

        subsession->sink = iptvMediaSink;
        if (!subsession->sink)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("IPTV # Failed to create sink: %1")
                    .arg(_live_env->getResultMsg()));
        }

        vector<TSDataListener*>::iterator it = _listeners.begin();
        for (; it != _listeners.end(); ++it)
            iptvMediaSink->AddListener(*it);

        subsession->sink->startPlaying(*(subsession->readSource()),
                                       sub_after_playing_cb,
                                       new RTSPData(subsession));

        if (subsession->rtcpInstance())
        {
            subsession->rtcpInstance()->setByeHandler(
                sub_bye_handler_cb, new RTSPData(subsession));
        }

        madeProgress = true;
    }

    if (!madeProgress)
        return false;

    // Setup player
    if (!(_rtsp_client->playMediaSession(*_session)))
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Failed to start playing session: %1")
                .arg(_live_env->getResultMsg()));
        return false;
    }

    VERBOSE(VB_RECORD, LOC + "Open() -- end");
    return true;
}

void IPTVFeederRTSP::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");
    Stop();

    VERBOSE(VB_RECORD, LOC + "Close() -- middle 1");

    _lock.lock();
    if (_session)
    {
        // Ensure RTSP cleanup, remove old RTSP session
        MediaSubsessionIterator iter(*_session);
        MediaSubsession *subsession;
        while ((subsession = iter.next())) /*<-extra braces for pedantic gcc*/
        {
            Medium::close(subsession->sink);
            subsession->sink = NULL;
        }

        _rtsp_client->teardownMediaSession(*_session);

        // Close all RTSP descriptor
        Medium::close(_session);
        _session = NULL;
    }
    _lock.unlock();

    if (_rtsp_client)
    {
        Medium::close(_rtsp_client);
        _rtsp_client = NULL;
    }

    FreeEnv();

    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void IPTVFeederRTSP::AddListener(TSDataListener *item)
{
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- begin");
    if (!item)
    {
        VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end 0");
        return;
    }

    // avoid duplicates
    RemoveListener(item);

    // add to local list
    QMutexLocker locker(&_lock);
    _listeners.push_back(item);

    // if there is a session, add to each subsession->sink
    if (!_session)
    {
        VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end 1");
        return;
    }

    MediaSubsessionIterator mit(*_session);
    MediaSubsession *subsession;
    while ((subsession = mit.next())) /* <- extra braces for pedantic gcc */
    {
        IPTVMediaSink *sink = NULL;
        if ((sink = dynamic_cast<IPTVMediaSink*>(subsession->sink)))
            sink->AddListener(item);
    }
    VERBOSE(VB_RECORD, LOC + "AddListener("<<item<<") -- end 2");
}

void IPTVFeederRTSP::RemoveListener(TSDataListener *item)
{
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- begin");
    QMutexLocker locker(&_lock);
    vector<TSDataListener*>::iterator it =
        find(_listeners.begin(), _listeners.end(), item);

    if (it == _listeners.end())
    {
        VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 1");
        return;
    }

    // remove from local list..
    *it = *_listeners.rbegin();
    _listeners.resize(_listeners.size() - 1);

    // if there is a session, remove from each subsession->sink
    if (!_session)
    {
        VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 2");
        return;
    }

    MediaSubsessionIterator mit(*_session);
    MediaSubsession *subsession;
    while ((subsession = mit.next())) /* <- extra braces for pedantic gcc */
    {
        IPTVMediaSink *sink = NULL;
        if ((sink = dynamic_cast<IPTVMediaSink*>(subsession->sink)))
            sink->RemoveListener(item);
    }
    VERBOSE(VB_RECORD, LOC + "RemoveListener("<<item<<") -- end 3");
}
