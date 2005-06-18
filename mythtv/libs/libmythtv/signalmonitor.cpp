// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "mythcontext.h"
#include "signalmonitor.h"
#include "pchdtvsignalmonitor.h"
#include "channel.h"

#ifdef USING_DVB
#   include "dvbsignalmonitor.h"
#   include "dvbchannel.h"
#endif

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

bool SignalMonitor::IsSupported(QString cardtype)
{
#ifdef USING_DVB
    if (cardtype.upper() == "DVB")
        return true;
#endif
    if (cardtype.upper() == "HDTV")
        return true;

    return false;
}

SignalMonitor *SignalMonitor::Init(QString cardtype, int db_cardnum,
                                   ChannelBase *channel)
{
    SignalMonitor *signalMonitor = NULL;

    if (cardtype.upper() == "DVB")
    {
#ifdef USING_DVB
        DVBChannel *dvbc = dynamic_cast<DVBChannel*>(channel);
        if (dvbc)
            signalMonitor = new DVBSignalMonitor(db_cardnum, dvbc);
#endif
    }

    if (cardtype.upper() == "HDTV")
    {
        Channel *hdtvc = dynamic_cast<Channel*>(channel);
        if (hdtvc)
            signalMonitor = new pcHDTVSignalMonitor(db_cardnum, hdtvc);
    }

    return signalMonitor;
}

/** \fn SignalMonitor::SignalMonitor(int,int)
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring.
 *
 *  \param capturecardnum recorder number to monitor,
 *         if this is less than 0, SIGNAL events will not be
 *         sent to the frontend even if SetNotifyFrontend(true)
 *         is called.
 *  \param fd file descriptor for device being monitored.
 *  \param wait_for_mask SignalMonitorFlags to start with.
 */
SignalMonitor::SignalMonitor(int _capturecardnum, int _fd, uint wait_for_mask,
                             const char *name)
    : QObject(NULL, name),
      capturecardnum(_capturecardnum), fd(_fd), flags(wait_for_mask),
      update_rate(25), running(false), exit(false), update_done(false),
      notify_frontend(true),
      signalLock(QObject::tr("Signal Lock"), "slock", 1, true, 0, 1, 0),
      signalStrength(QObject::tr("Signal Power"), "signal", 0, true, 0, 100, 0)
{
}

/** \fn SignalMonitor::~SignalMonitor()
 *  \brief Stops monitoring thread.
 */
SignalMonitor::~SignalMonitor()
{
    Stop();
}

/** \fn SignalMonitor::Start()
 *  \brief Start signal monitoring thread.
 */
void SignalMonitor::Start()
{
    if (!running)
        pthread_create(&monitor_thread, NULL, SpawnMonitorLoop, this);
    while (!running)
        usleep(50);
}

/** \fn SignalMonitor::Stop()
 *  \brief Stop signal monitoring thread.
 */
void SignalMonitor::Stop()
{
    if (running)
    {
        exit = true;
        pthread_join(monitor_thread, NULL);
    }
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
    list<<signalLock.GetName()<<signalLock.GetStatus();
    if (HasFlags(kDTVSigMon_WaitForSig))
        list<<signalStrength.GetName()<<signalStrength.GetStatus();

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
    if (-1 == timeout)
        timeout = signalLock.GetTimeout();
    if (timeout<0)
        return false;

    QTime t;
    t.start();
    if (running)
    {
        while (t.elapsed()<timeout && running)
        {
            Kick();
            if (signalLock.IsGood())
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
            if (signalLock.IsGood())
                return true;

            usleep(50);
        }
        if (running)
            return WaitForLock(timeout-t.elapsed());
    }
    return false;
}
