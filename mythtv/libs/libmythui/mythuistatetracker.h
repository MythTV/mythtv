#ifndef MYTHUISTATETRACKER_H
#define MYTHUISTATETRACKER_H

#include <QVariantMap>
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
#include <QMutex>
#else
#include <QRecursiveMutex>
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    static inline QMutex gUIStateLock { QMutex::Recursive };
#else
    static inline QRecursiveMutex gUIStateLock;
#endif

    MythUIStateTracker();
    QVariantMap m_state;
    QTime       m_lastUpdated;
};

#endif
