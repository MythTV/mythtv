#include <unistd.h>
#include <QCoreApplication>
#include "mythmainwindow.h"
#include "mythevent.h"
#include "mythuistatetracker.h"
#include "mythuiactions.h"

MythUIStateTracker* MythUIStateTracker::gUIState = NULL;
QMutex* MythUIStateTracker::gUIStateLock = new QMutex();

MythUIStateTracker* MythUIStateTracker::GetMythUIStateTracker(void)
{
    gUIStateLock->lock();
    if (!gUIState)
        gUIState = new MythUIStateTracker();
    gUIStateLock->unlock();
    return gUIState;
}

void MythUIStateTracker::SetState(QVariantMap &newstate)
{
    MythUIStateTracker* uistate = MythUIStateTracker::GetMythUIStateTracker();
    gUIStateLock->lock();
    uistate->m_state = newstate;
    uistate->m_state.detach();
    uistate->m_lastUpdated = QTime::currentTime();
    gUIStateLock->unlock();
}

void MythUIStateTracker::GetState(QVariantMap &state)
{
    MythUIStateTracker* uistate = MythUIStateTracker::GetMythUIStateTracker();
    gUIStateLock->lock();
    state = uistate->m_state;
    state.detach();
    gUIStateLock->unlock();
}

void MythUIStateTracker::GetFreshState(QVariantMap &state)
{
    if (MythUIStateTracker::TimeSinceLastUpdate() < 500)
    {
        MythUIStateTracker::GetState(state);
        return;
    }

    MythEvent *e = new MythEvent(ACTION_GETSTATUS);
    qApp->postEvent(GetMythMainWindow(), e);

    int tries = 0;
    while ((tries++ < 100) && (MythUIStateTracker::TimeSinceLastUpdate() >= 500))
        usleep(10000);

    MythUIStateTracker::GetState(state);
}

int MythUIStateTracker::TimeSinceLastUpdate(void)
{
    MythUIStateTracker* state = MythUIStateTracker::GetMythUIStateTracker();
    gUIStateLock->lock();
    int age = state->m_lastUpdated.msecsTo(QTime::currentTime());
    gUIStateLock->unlock();
    return age < 0 ? 1000000 : age;
}
