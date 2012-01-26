#ifndef MYTHUISTATETRACKER_H
#define MYTHUISTATETRACKER_H

#include <QVariantMap>
#include <QMutex>
#include <QTime>

#include "mythuiexp.h"

class MUI_PUBLIC MythUIStateTracker
{
  public:
    static void SetState(QVariantMap &newstate);
    static void GetState(QVariantMap &state);
    static void GetFreshState(QVariantMap &state);

  protected:
    static MythUIStateTracker* GetMythUIStateTracker(void);
    static int                 TimeSinceLastUpdate(void);
    static MythUIStateTracker *gUIState;
    static QMutex             *gUIStateLock;

    MythUIStateTracker(): m_lastUpdated(QTime::currentTime().addSecs(-1)) { }

    QVariantMap m_state;
    QTime       m_lastUpdated;
};

#endif // MYTHUISTATETRACKER_H
