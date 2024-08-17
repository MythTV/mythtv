// Qt headers

// MythTV headers
#include <libmyth/mythcontext.h>

// MythZoneMinder headers
#include "alarmnotifythread.h"
#include "zmclient.h"
#include "zmdefines.h"

class AlarmNotifyThread *AlarmNotifyThread::m_alarmNotifyThread = nullptr;

class AlarmNotifyThread *AlarmNotifyThread::get(void)
{
    if (m_alarmNotifyThread == nullptr)
        m_alarmNotifyThread = new AlarmNotifyThread;
    return m_alarmNotifyThread;
}

AlarmNotifyThread::AlarmNotifyThread()
  : MThread("AlarmNotifyThread")
{
}

AlarmNotifyThread::~AlarmNotifyThread()
{
    stop();
}

void AlarmNotifyThread::stop (void)
{
    if (isRunning())
    {
        m_stop = true;
        wait();
    }
}

void AlarmNotifyThread::run()
{
    RunProlog();

    while (!m_stop)
    {
        // get the alarm status for all monitors
        if (ZMClient::get()->connected() && ZMClient::get()->updateAlarmStates())
        {
            // at least one monitor changed state
            for (int x = 0; x < ZMClient::get()->getMonitorCount(); x++)
            {
                Monitor *mon = ZMClient::get()->getMonitorAt(x);

                if (mon)
                {
                    if (mon->previousState != mon->state && (mon->state == ALARM || (mon->state == ALERT && mon->previousState != ALARM)))
                    {
                        // have notifications been turned on for this monitor?
                        if (mon->showNotifications)
                        {
                            // we can't show a popup from the AlarmNotifyThread so send
                            // a message to ZMClient to do it for us
                            gCoreContext->dispatch(MythEvent(QString("ZONEMINDER_NOTIFICATION %1").arg(mon->id)));
                        }
                    }
                }
            }
        }

        const struct timespec onesec {1, 0};
        nanosleep(&onesec, nullptr);
    }

    RunEpilog();
}
