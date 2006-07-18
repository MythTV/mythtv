/** -*- Mode: c++ -*-
 *  RTSPComms
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "rtspcomms.h"

// Live555 headers
#include <RTSPClient.hh>
#include <BasicUsageEnvironment.hh>
#include <MediaSession.hh>

// MythTV headers
#include "freeboxmediasink.h"
#include "mythcontext.h"
#include "tspacket.h"

#define LOC QString("RTSPData:")
#define LOC_ERR QString("RTSPData, Error:")

// ============================================================================
// RTSPData : Helper class use for static Callback handler
// ============================================================================
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

    MediaSession &session = subsession->parentSession();
    MediaSubsessionIterator iter(session);

    while ((subsession = iter.next())) /* <- extra braces for pedantic gcc */
    {
        if (subsession->sink)
            return;
    }
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

RTSPComms::RTSPComms(RTSPListener *recorder) :
    _abort(0),          _running(false),
    _rec(recorder),
    _live_env(NULL),    _rtsp_client(NULL),
    _session(NULL)
{
    //Init();
}

RTSPComms::~RTSPComms()
{
    VERBOSE(VB_RECORD, LOC + "dtor -- begin");
    //Stop();
    Close();
    //Deinit();
    VERBOSE(VB_RECORD, LOC + "dtor -- end");
}

bool RTSPComms::Init(void)
{
    VERBOSE(VB_RECORD, LOC + "Init() -- begin");
    QMutexLocker locker(&_lock);

    if (_rtsp_client)
        return true;

    // Begin by setting up our usage environment:
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    if (!scheduler)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create Live Scheduler.");
        return false;
    }

    _live_env = BasicUsageEnvironment::createNew(*scheduler);
    if (!_live_env)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create Live Environment.");
        delete scheduler;
        return false;
    }

    // Create our client object:
    _rtsp_client = RTSPClient::createNew(*_live_env, 0, "myRTSP", 0);

    if (!_rtsp_client)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to create RTSP client: %1")
                .arg(_live_env->getResultMsg()));

        _live_env->reclaim();
        _live_env = NULL;
        delete scheduler;
    }

    VERBOSE(VB_RECORD, LOC + "Init() -- end");
    return _rtsp_client;
}

void RTSPComms::Deinit(void)
{
    VERBOSE(VB_RECORD, LOC + "Deinit() -- begin");

    if (_session)
        Close();

    if (_rtsp_client)
    {
        Medium::close(_rtsp_client);
        _rtsp_client = NULL;
    }

    if (_live_env)
    {
        TaskScheduler *scheduler = &_live_env->taskScheduler();
        _live_env->reclaim();
        _live_env = NULL;
        delete scheduler;
    }
    VERBOSE(VB_RECORD, LOC + "Deinit() -- end");
}

bool RTSPComms::Open(const QString &url)
{
    VERBOSE(VB_RECORD, LOC + "Open() -- begin");
    if (!_rec)
        return false;

    if (!Init())
        return false;

    QMutexLocker locker(&_lock);

    // Setup URL for the current session
    char *sdpDescription = _rtsp_client->describeURL(url);
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

        FreeboxMediaSink *freeboxMediaSink = FreeboxMediaSink::CreateNew(
            *_live_env, *_rec, TSPacket::SIZE * 128*1024);

        subsession->sink = freeboxMediaSink;
        if (!subsession->sink)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Freebox # Failed to create sink: %1")
                    .arg(_live_env->getResultMsg()));
        }

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

void RTSPComms::Close(void)
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

    VERBOSE(VB_RECORD, LOC + "Close() -- middle 2");

    Deinit();
    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void RTSPComms::Run(void)
{
    VERBOSE(VB_RECORD, LOC + "Run() -- begin");
    _lock.lock();
    _running = true;
    _abort   = 0;
    _lock.unlock();

    VERBOSE(VB_RECORD, LOC + "Run() -- loop begin");
    if (_live_env)
        _live_env->taskScheduler().doEventLoop(&_abort);
    VERBOSE(VB_RECORD, LOC + "Run() -- loop end");

    _lock.lock();
    _running = false;
    _cond.wakeAll();
    _lock.unlock();
    VERBOSE(VB_RECORD, LOC + "Run() -- end");
}

void RTSPComms::Stop(void)
{
    VERBOSE(VB_RECORD, LOC + "Stop() -- begin");
    QMutexLocker locker(&_lock);
    _abort = 0xFF;

    while (_running)
        _cond.wait(&_lock, 500);
    VERBOSE(VB_RECORD, LOC + "Stop() -- end");
}
