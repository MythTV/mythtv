#ifndef MYTHUISTATETRACKER_H
#define MYTHUISTATETRACKER_H

#include <QVariantMap>
#include <QRecursiveMutex>
#include <QTime>

#include "mythuiexp.h"

class MUI_PUBLIC MythUIStateTracker
{
  public:
    static void SetState(const QVariantMap& NewState);
    static void GetState(QVariantMap& State);
    static void GetFreshState(QVariantMap& State);

  protected:
    static MythUIStateTracker* GetMythUIStateTracker();
    static std::chrono::milliseconds TimeSinceLastUpdate();
    static inline MythUIStateTracker* gUIState { nullptr };
    static inline QRecursiveMutex gUIStateLock;

    MythUIStateTracker();
    QVariantMap m_state;
    QTime       m_lastUpdated;
};

#endif
