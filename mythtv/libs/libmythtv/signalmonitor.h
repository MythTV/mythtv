#ifndef SIGNALMONITOR_H
#define SIGNALMONITOR_H

#include "signalmonitorvalue.h"
#include <pthread.h>
#include <qobject.h>

class SignalMonitor: public QObject
{
    Q_OBJECT
public:
    SignalMonitor(int capturecardnum, int fd);
    virtual ~SignalMonitor();
    
    // // // // // // // // // // // // // // // // // // // // // // // //
    // Control  // // // // // // // // // // // // // // // // // // // //

    void Start();
    void Stop();
    void Kick();
    virtual bool WaitForLock(int timeout = -1);

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Gets  // // // // // // // // // // // // // // // // // // // // //

    /// \brief Returns whether or not a SIGNAL MythEvent is being sent
    ///        regularly to the frontend.
    bool GetNotifyFrontend() { return notify_frontend; }
    /// \brief Returns milliseconds between signal monitoring events.
    int GetUpdateRate() { return update_rate; }
    virtual QStringList GetStatusList(bool kick = true);


    // // // // // // // // // // // // // // // // // // // // // // // //
    // Sets  // // // // // // // // // // // // // // // // // // // // //

    /** \brief Enables or disables frontend notification of the current
     *         signal value.
     *  \param notify if true SIGNAL MythEvents are sent to the frontend,
     *         otherwise they are not.
     */
    void SetNotifyFrontend(bool notify) { notify_frontend = notify; }

    /** \brief Sets the number of milliseconds between signal monitoring
     *         attempts in the signal monitoring thread.
     *
     *   Defaults to 25 milliseconds.
     *  \param msec Milliseconds between signal monitoring events.
     */
    void SetUpdateRate(int msec) { update_rate = msec; }

signals:
    /** \brief Signal to be sent as true when it is safe to begin
     *   or continue recording, and false if it may not be safe.
     *
     *   Note: Signals are only sent once the monitoring thread has been started.
     */
    void StatusSignalLock(int);

    /** \brief Signal to be sent as with an actual signal value.
     *
     *   Note: Signals are only sent once the monitoring thread has been started.
     */
    void StatusSignalStrength(int);

protected:
    static void* SpawnMonitorLoop(void*);
    virtual void MonitorLoop();

    /// \brief This should be overriden to actually do signal monitoring.
    virtual void UpdateValues() { ; }

    pthread_t   monitor_thread;
    int         capturecardnum;
    int         fd;
    int         update_rate;
    bool        running;
    bool        exit;
    bool        update_done;
    bool        notify_frontend;

    SignalMonitorValue signalLock;
    SignalMonitorValue signalStrength;
};

#endif // SIGNALMONITOR_H
