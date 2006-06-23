/**
 *  FreeboxRecorder
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// Qt headers
#include <qwaitcondition.h>
#include <qmutex.h>

// Live555 headers
#include <BasicUsageEnvironment.hh>
#include <MediaSession.hh>
#include <RTSPClient.hh>

// MythTV headers
#include "mpeg/mpegstreamdata.h"
#include "mpeg/streamlisteners.h"
#include "mpeg/tspacket.h"
#include "freeboxchannel.h"
#include "freeboxmediasink.h"
#include "freeboxrecorder.h"

// ============================================================================
// FreeboxData : Helper class use for static Callback handler
// ============================================================================
class FreeboxData
{
  public:
    FreeboxData(FreeboxRecorder *pFreeboxRecorder,
                MediaSubsession *pMediaSubSession) :
        freeboxRecorder(pFreeboxRecorder),
        mediaSubSession(pMediaSubSession)
    {
    }

    void SubsessionAfterPlayingCB(void);
    void SubsessionByeHandlerCB(void);

  private:
    FreeboxRecorder *freeboxRecorder;
    MediaSubsession *mediaSubSession;
};

void FreeboxData::SubsessionAfterPlayingCB(void)
{
    MediaSubsession* subsession = mediaSubSession;
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);

    while ((subsession = iter.next())) /* <- extra braces for pedantic gcc */
    {
        if (subsession->sink)
            return;
    }
}

static void sub_after_playing_cb(void *clientData)
{
    ((FreeboxData*)clientData)->SubsessionAfterPlayingCB();
}

void FreeboxData::SubsessionByeHandlerCB(void)
{
    SubsessionAfterPlayingCB();
}

static void sub_bye_handler_cb(void *clientData)
{
    ((FreeboxData*)clientData)->SubsessionByeHandlerCB();
}

class FreeboxRecorderImpl : public MPEGSingleProgramStreamListener
{
  public:
    FreeboxRecorderImpl(FreeboxRecorder *recorder,
                        FreeboxChannel  *channel) :
        _rec(recorder),
        _streamData(1, true),
        _curChanInfo(channel->GetCurrentChanInfo()),
        _live_env(NULL),
        _rtsp_client(NULL),
        _session(NULL),
        _channel(channel)
    {
        _streamData.AddMPEGSPListener(this);
        _channel->SetRecorder(_rec);
    }

    virtual ~FreeboxRecorderImpl()
    {
        _channel->SetRecorder(NULL);
    }

    void Close(void);

    void Lock(void)   const { _lock.lock();    }
    void Unlock(void) const { _lock.unlock();  }
    void WakeAll(void)      { _cond.wakeAll(); }
    void Wait(int msec)     { _cond.wait(&_lock, msec); }

    // Gets
    FreeboxChannelInfo GetCurrentChanInfo(void) const
        { return _curChanInfo; }
    MPEGStreamData&    StreamData(void)
        { return _streamData; }
    UsageEnvironment*  GetLiveEnv(void)
        { return _live_env; }
    RTSPClient*        GetRTSPClient(void)
        { return _rtsp_client; }
    MediaSession*      GetSession(void)
        { return _session; }

    // Sets
    void SetLiveEnv(UsageEnvironment *env) { _live_env    = env;     }
    void SetRTSPClient(RTSPClient *client) { _rtsp_client = client;  }
    void SetSession(MediaSession *session) { _session     = session; }
    void SetChannelInfo(const FreeboxChannelInfo& chanInfo)
        { _curChanInfo = chanInfo; }

  public: // MPEGSingleProgramStreamListener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable *pmt);

  private:
    FreeboxRecorder    *_rec;
    MPEGStreamData      _streamData;
    FreeboxChannelInfo  _curChanInfo;
    UsageEnvironment   *_live_env;
    RTSPClient         *_rtsp_client;
    MediaSession       *_session;
    FreeboxChannel     *_channel;    ///< Current channel
    QWaitCondition      _cond;       ///< Condition  used to coordinate threads
    mutable QMutex      _lock;       ///< Lock  used to coordinate threads
};

void FreeboxRecorderImpl::HandleSingleProgramPAT(ProgramAssociationTable *pat)
{
    if (!pat)
        return;

    int next = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next);
    _rec->BufferedWrite(*(reinterpret_cast<const TSPacket*>(pat->tsheader())));
}

void FreeboxRecorderImpl::HandleSingleProgramPMT(ProgramMapTable *pmt)
{
    if (!pmt)
        return;

    int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next);
    _rec->BufferedWrite(*(reinterpret_cast<const TSPacket*>(pmt->tsheader())));
}

void FreeboxRecorderImpl::Close(void)
{
    if (!_session)
        return;

    // Ensure RTSP cleanup, remove old RTSP session
    MediaSubsessionIterator iter(*_session);
    MediaSubsession *subsession;
    while ((subsession = iter.next())) /* <- extra braces for pedantic gcc */
    {
        Medium::close(subsession->sink);
        subsession->sink = NULL;
    }

    if (!_session)
        return;

    _rtsp_client->teardownMediaSession(*_session);

    // Close all RTSP descriptor
    Medium::close(_session);
    Medium::close(_rtsp_client);
}

FreeboxRecorder::FreeboxRecorder(TVRec *rec, FreeboxChannel *channel) :
    DTVRecorder(rec), _impl(new FreeboxRecorderImpl(this, channel)),
    _abort_rtsp(0), _abort_recording(false)
{
}

FreeboxRecorder::~FreeboxRecorder()
{
    delete _impl;
}

bool FreeboxRecorder::Open(void)
{
    return !(_error = !StartRtsp());
}

void FreeboxRecorder::Close(void)
{
    _impl->Close();
}

void FreeboxRecorder::ChannelChanged(const FreeboxChannelInfo &chanInfo)
{
    // keep a copy to avoid deadlocks (TODO FIXME why is this needed?)
    _impl->SetChannelInfo(chanInfo);
    // Channel changed, we need to close current RTSP flow, and open a new one
    ResetEventLoop();
}

void FreeboxRecorder::StartRecording(void)
{
    _impl->Lock();
    _recording = true;

    while (!_abort_recording && Open())
    {
        _impl->Unlock();

        // Go into main RTSP loop, feeding data to mythtv
        _impl->GetLiveEnv()->taskScheduler().doEventLoop(&_abort_rtsp);

        _impl->Lock();
        FinishRecording();
        Close();

        // Reset _abort_rtsp before unlocking ResetEventLoop()
        // to avoid race condition
        _abort_rtsp = 0;
        _impl->WakeAll();
    }

    _recording = false;
    _impl->Unlock();
}

void FreeboxRecorder::StopRecording(void)
{
    _abort_recording = true; // No lock needed
    ResetEventLoop();
}

void FreeboxRecorder::ResetEventLoop(void)
{
    _impl->Lock();
    _abort_rtsp = ~0;

    while (_recording && _abort_rtsp)
        _impl->Wait(500);

    _impl->Unlock();
}

// ======================================================================
// StartRtsp : start a new RTSP session for the current channel
// ======================================================================
bool FreeboxRecorder::StartRtsp(void)
{
    // Retrieve the RTSP channel URL
    FreeboxChannelInfo chaninfo = _impl->GetCurrentChanInfo();

    if (!chaninfo.isValid())
        return false;

    QString url = chaninfo.m_url;

    // Begin by setting up our usage environment:
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    _impl->SetLiveEnv(BasicUsageEnvironment::createNew(*scheduler));

    // Create our client object:
    RTSPClient *tmp = RTSPClient::createNew(*_impl->GetLiveEnv(), 0, "myRTSP", 0);
    _impl->SetRTSPClient(tmp);
    if (!tmp)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Freebox # Failed to create RTSP client: %1")
                .arg(_impl->GetLiveEnv()->getResultMsg()));
        return false;
    }

    // Setup URL for the current session
    char *sdpDescription = _impl->GetRTSPClient()->describeURL(url);
    _impl->GetRTSPClient()->describeStatus();

    if (!sdpDescription)
    {
        VERBOSE(VB_IMPORTANT, QString(
                    "Freebox # Failed to get a SDP description from URL: %1 %2")
                .arg(url).arg(_impl->GetLiveEnv()->getResultMsg()));
        return false;
    }

    // Create a media session object from this SDP description:
    _impl->SetSession(MediaSession::createNew(
                          *_impl->GetLiveEnv(), sdpDescription));

    delete[] sdpDescription;

    if (!_impl->GetSession())
    {
        VERBOSE(VB_IMPORTANT,
                QString("Freebox # Failed to create MediaSession: %1")
                .arg(_impl->GetLiveEnv()->getResultMsg()));
        return false;
    }
    else if (!_impl->GetSession()->hasSubsessions())
    {
        VERBOSE(VB_IMPORTANT,
                QString("Freebox # This session has no media subsessions"));
        return false;
    }

    // Then, setup the "RTPSource"s for the session:
    MediaSubsessionIterator iter(*_impl->GetSession());
    MediaSubsession *subsession;
    bool madeProgress = false;

    while ((subsession = iter.next())) /* <- extra braces for pedantic gcc */
    {
        if (!subsession->initiate(-1))
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "Freebox # Can't create receiver for: %1 / %2 subsession: %3")
                    .arg(subsession->mediumName())
                    .arg(subsession->codecName())
                    .arg(_impl->GetLiveEnv()->getResultMsg()));
        }
        else
        {
            madeProgress = true;

            if (subsession->rtpSource() != NULL)
            {
                unsigned const thresh = 1000000; // 1 second
                subsession->rtpSource()->setPacketReorderingThresholdTime(thresh);
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

        if (_impl->GetRTSPClient()->setupMediaSubsession(*subsession, false, false))
        {
            madeProgress = true;
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Freebox # Failed to setup: %1 %2 : %3")
                    .arg(subsession->mediumName())
                    .arg(subsession->codecName())
                    .arg(_impl->GetLiveEnv()->getResultMsg()));
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

        FreeboxMediaSink *freeboxMediaSink = FreeboxMediaSink::createNew(
            *_impl->GetLiveEnv(), *this, TSPacket::SIZE * 128);

        subsession->sink = freeboxMediaSink;
        if (!subsession->sink)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Freebox # Failed to create sink: %1")
                    .arg(_impl->GetLiveEnv()->getResultMsg()));
        }

        subsession->sink->startPlaying(*(subsession->readSource()),
                                       sub_after_playing_cb,
                                       new FreeboxData(this, subsession));

        if (subsession->rtcpInstance())
        {
            subsession->rtcpInstance()->setByeHandler(
                sub_bye_handler_cb, new FreeboxData(this, subsession));
        }

        madeProgress = true;
    }

    if (!madeProgress)
        return false;

    // Setup player
    if (!(_impl->GetRTSPClient()->playMediaSession(*_impl->GetSession())))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Freebox # Failed to start playing session: %1")
                .arg(_impl->GetLiveEnv()->getResultMsg()));
        return false;
    }

    return true;
}

// ===================================================
// findTSHeader : find a TS Header in flow
// ===================================================
static int FreeboxRecorder_findTSHeader(const unsigned char *data,
                                        unsigned int         dataSize)
{
    unsigned int pos = 0;

    while (pos < dataSize)
    {
        if (data[pos] == 0x47)
            return pos;
        pos++;
    }

    return -1;
}

// ===================================================
// addData : feed date from RTSP flow to mythtv
// ===================================================
void FreeboxRecorder::AddData(unsigned char *data,
                              unsigned       dataSize,
                              struct timeval)
{
    unsigned int readIndex = 0;

    // data may be compose from more than one packet, loop to consume all data
    while (readIndex < dataSize)
    {
        // If recorder is pause, stop there
        if (PauseAndWait())
        {
            return;
        }

        // Find the next TS Header in data
        int tsPos = FreeboxRecorder_findTSHeader(data + readIndex, dataSize);

        // if no TS, something bad happens
        if (tsPos == -1)
        {
            VERBOSE(VB_IMPORTANT, "FREEBOX: No TS header.");
            break;
        }

        // if TS Header not at start of data, we receive out of sync data
        if (tsPos > 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("FREEBOX: TS header at %1, not in sync.")
                    .arg(tsPos));
        }

        // Check if the next packet in buffer is complete :
        // packet size is 188 bytes long
        if ((dataSize - tsPos) < TSPacket::SIZE)
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "FREEBOX: TS header at %1 but packet not yet complete.")
                    .arg(tsPos));
            break;
        }

        // Cast current found TS Packet to TSPacket structure
        const void *newData = data + tsPos + readIndex;
        ProcessTSPacket(*reinterpret_cast<const TSPacket*>(newData));

        // follow to next packet
        readIndex += tsPos + TSPacket::SIZE;
    }
}

void FreeboxRecorder::ProcessTSPacket(const TSPacket& tspacket)
{
    if (tspacket.TransportError())
        return;

    if (tspacket.ScramplingControl())
        return;

    MPEGStreamData &sd = _impl->StreamData();
    if (tspacket.HasAdaptationField())
        sd.HandleAdaptationFieldControl(&tspacket);

    if (tspacket.HasPayload())
    {
        const unsigned int lpid = tspacket.PID();

        // Pass or reject packets based on PID, and parse info from them
        if (lpid == sd.VideoPIDSingleProgram())
        {
            _buffer_packets = !FindMPEG2Keyframes(&tspacket);
            BufferedWrite(tspacket);
        }
        else if (sd.IsAudioPID(lpid))
            BufferedWrite(tspacket);
        else if (sd.IsListeningPID(lpid))
            sd.HandleTSTables(&tspacket);
        else if (sd.IsWritingPID(lpid))
            BufferedWrite(tspacket);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
