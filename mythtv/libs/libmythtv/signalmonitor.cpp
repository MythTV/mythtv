// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

// C headers
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

// MythTV headers
#include "mythcontext.h"
#include "signalmonitor.h"

#include "libavcodec/avcodec.h"

#ifdef USING_DVB
#   include "dvbsignalmonitor.h"
#   include "dvbchannel.h"
#endif

#ifdef USING_V4L
#   include "pchdtvsignalmonitor.h"
#   include "channel.h"
#endif

#undef DBG_SM
#define DBG_SM(FUNC, MSG) VERBOSE(VB_CHANNEL, \
    "SM("<<channel->GetDevice()<<")::"<<FUNC<<": "<<MSG);

/** \class SignalMonitor
 *  \brief Signal monitoring base class.
 *
 *   Signal monitors should extend this to add signal monitoring to their
 *   recorder. All signal monitors must implement one signals, the
 *   StatusSignalLock(int) signal. The lock signal should only be set to
 *   true when it is absolutely safe to begin or to continue recording.
 *   The optional StatusSignalStrength signal should report the actual
 *   signal value.
 *
 *   Additional signals may be implemented, see DTVSignalMonitor and 
 *   DVBSignalMonitor for example.
 *
 *  \sa DTVSignalMonitor DVBSignalMonitor, HDTVSignalMonitor, SignalMonitorValue
 */

void ALRMhandler(int /*sig*/)
{
     cerr<<"SignalMonitor: Got SIGALRM"<<endl;
     signal(SIGINT, ALRMhandler);
}

SignalMonitor *SignalMonitor::Init(QString cardtype, int db_cardnum,
                                   ChannelBase *channel)
{
    (void) cardtype;
    (void) db_cardnum;
    (void) channel;

    SignalMonitor *signalMonitor = NULL;

    {
        QMutexLocker locker(&avcodeclock);
        avcodec_init();
    }

#ifdef USING_DVB
    if (CardUtil::IsDVBCardType(cardtype))
    {
        DVBChannel *dvbc = dynamic_cast<DVBChannel*>(channel);
        if (dvbc)
            signalMonitor = new DVBSignalMonitor(db_cardnum, dvbc);
    }
#endif

#ifdef USING_V4L
    if (cardtype.upper() == "HDTV")
    {
        Channel *hdtvc = dynamic_cast<Channel*>(channel);
        if (hdtvc)
            signalMonitor = new pcHDTVSignalMonitor(db_cardnum, hdtvc);
    }
#endif
    if (!signalMonitor)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Failed to create signal monitor in Init(%1, %2, 0x%3)")
                .arg(cardtype).arg(db_cardnum).arg((long)channel,0,16));
    }

    return signalMonitor;
}

/** \fn SignalMonitor::SignalMonitor(int,ChannelBase*,uint,const char*)
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring.
 *
 *  \param db_cardnum Recorder number to monitor,
 *         if this is less than 0, SIGNAL events will not be
 *         sent to the frontend even if SetNotifyFrontend(true)
 *         is called.
 *  \param _channel      ChannelBase class for our monitoring
 *  \param wait_for_mask SignalMonitorFlags to start with.
 *  \param name          Instance name for Qt signal/slot debugging
 */
SignalMonitor::SignalMonitor(int _capturecardnum, ChannelBase *_channel,
                             uint wait_for_mask,  const char *name)
    : QObject(NULL, name),             channel(_channel),
      capturecardnum(_capturecardnum), flags(wait_for_mask),
      update_rate(25),                 minimum_update_rate(5),
      running(false),                  exit(false),
      update_done(false),              notify_frontend(true),
      signalLock    (QObject::tr("Signal Lock"),  "slock",
                     1, true, 0,   1, 0),
      signalStrength(QObject::tr("Signal Power"), "signal",
                     0, true, 0, 100, 0),
      statusLock(true)
{
}

/** \fn SignalMonitor::~SignalMonitor()
 *  \brief Stops monitoring thread.
 */
SignalMonitor::~SignalMonitor()
{
    Stop();
}

/** \fn SignalMonitor::deleteLater(void)
 *  \brief Safer alternative to just deleting signal monitor directly.
 */
void SignalMonitor::deleteLater(void)
{
    disconnect(); // disconnect signals we may be sending...
    Stop();
    QObject::deleteLater();
}

/** \fn SignalMonitor::Start()
 *  \brief Start signal monitoring thread.
 */
void SignalMonitor::Start()
{
    DBG_SM("Start", "begin");
    {
        QMutexLocker locker(&startStopLock);
        if (!running)
            pthread_create(&monitor_thread, NULL, SpawnMonitorLoop, this);
        while (!running)
            usleep(50);
    }
    DBG_SM("Start", "end");
}

/** \fn SignalMonitor::Stop()
 *  \brief Stop signal monitoring thread.
 */
void SignalMonitor::Stop()
{
    DBG_SM("Stop", "begin");
    {
        QMutexLocker locker(&startStopLock);
        if (running)
        {
            exit = true;
            pthread_join(monitor_thread, NULL);
        }
    }
    DBG_SM("Stop", "end");
}

/** \fn SignalMonitor::Kick()
 *  \brief Wake up monitor thread, and wait for
 *         UpdateValue() to execute once.
 */
void SignalMonitor::Kick()
{
    update_done = false;

    //pthread_kill(monitor_thread, SIGALRM);

    while (!update_done)
        usleep(50);
}

/** \fn SignalMonitor::GetStatusList(bool)
 *  \brief Returns QStringList containing all signals and their current
 *         values.
 *
 *   This searlializes the signal monitoring values so that they can
 *   be passed from a backend to a frontend.
 *
 *   SignalMonitorValue::Parse(const QStringList&) will convert this
 *   to a vector of SignalMonitorValue instances.
 *
 *  \param kick if true Kick() will be employed so that this
 *         call will not have to wait for the next signal
 *         monitoring event.
 */
QStringList SignalMonitor::GetStatusList(bool kick)
{
    if (kick && running)
        Kick();
    else if (!running)
        UpdateValues();

    QStringList list;
    statusLock.lock();
    list<<signalLock.GetName()<<signalLock.GetStatus();
    if (HasFlags(kDTVSigMon_WaitForSig))
        list<<signalStrength.GetName()<<signalStrength.GetStatus();
    statusLock.unlock();

    return list;
}

/** \fn SignalMonitor::MonitorLoop()
 *  \brief Basic signal monitoring loop
 */
void SignalMonitor::MonitorLoop()
{
    //signal(SIGALRM, ALRMhandler);

    running = true;
    exit = false;

    while (!exit)
    {
        UpdateValues();

        if (notify_frontend && capturecardnum>=0)
        {
            QStringList slist = GetStatusList(false);
            MythEvent me(QString("SIGNAL %1").arg(capturecardnum), slist);
            gContext->dispatch(me);
            //cerr<<"sent SIGNAL"<<endl;
        }

        usleep(update_rate * 1000);
    }

    // We need to send a last informational message because a
    // signal update may have come in while we were sleeping
    // if we are using the multithreaded dtvsignalmonitor.
    if (notify_frontend && capturecardnum>=0)
    {
        QStringList slist = GetStatusList(false);
        MythEvent me(QString("SIGNAL %1").arg(capturecardnum), slist);
        gContext->dispatch(me);
    }

    //signal(SIGALRM, SIG_DFL);

    running = false;
}

/** \fn SignalMonitor::SpawnMonitorLoop(void*)
 *  \brief Runs MonitorLoop() within the monitor_thread pthread.
 */
void* SignalMonitor::SpawnMonitorLoop(void* self)
{
    ((SignalMonitor*)self)->MonitorLoop();
    return NULL;
}

/** \fn  SignalMonitor::WaitForLock(int)
 *  \brief Wait for a StatusSignaLock(int) of true.
 *
 *   This can be called whether or not the signal
 *   monitoring thread has been started.
 *
 *  \param timeout maximum time to wait in milliseconds.
 *  \return true if signal was acquired.
 */
bool SignalMonitor::WaitForLock(int timeout)
{
    statusLock.lock();
    if (-1 == timeout)
        timeout = signalLock.GetTimeout();
    statusLock.unlock();
    if (timeout<0)
        return false;

    MythTimer t;
    t.start();
    if (running)
    {
        while (t.elapsed()<timeout && running)
        {
            Kick();
            statusLock.lock();
            bool ok = signalLock.IsGood();
            statusLock.unlock();
            if (ok)
                return true;

            usleep(50);
        }
        if (!running)
            return WaitForLock(timeout-t.elapsed());
    }
    else
    {
        while (t.elapsed()<timeout && !running)
        {
            UpdateValues();
            statusLock.lock();
            bool ok = signalLock.IsGood();
            statusLock.unlock();
            if (ok)
                return true;

            usleep(50);
        }
        if (running)
            return WaitForLock(timeout-t.elapsed());
    }
    return false;
}
