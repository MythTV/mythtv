// -*- Mode: c++ -*-

#include <QString>

#include "referencecounter.h"
#include "mythlogging.h"

#ifdef EXTRA_DEBUG
// Uncomment this to see missing DecrRefs()
//#define LEAK_DEBUG
// Uncoment this to see extra DecrRefs(), no memory will ever be freed..
//#define NO_DELETE_DEBUG
#endif

#ifdef LEAK_DEBUG
#include <QMutex>
#include <QMap>
#include <cassert>
#undef NDEBUG
static QMutex leakLock;
struct LeakInfo
{
    LeakInfo() : refCount(0) {}
    LeakInfo(const QString &n) : name(n), refCount(1) {}
    QString name;
    uint refCount;
};
static QMap<ReferenceCounter*,LeakInfo> leakMap;
void ReferenceCounter::PrintDebug(void)
{
    QMutexLocker locker(&leakLock);
    LOG(VB_REFCOUNT, LOG_INFO, QString("Leaked %1 reference counted objects")
        .arg(leakMap.size()));
    QMap<ReferenceCounter*,LeakInfo>::iterator it = leakMap.begin();
    for (; it != leakMap.end(); ++it)
    {
        LOG(VB_REFCOUNT, LOG_INFO,
            QString("  Leaked %1(0x%2) reference count %3")
            .arg((*it).name)
            .arg(reinterpret_cast<intptr_t>(it.key()),0,16)
            .arg((*it).refCount));
    }
}
#else
void ReferenceCounter::PrintDebug(void) {}
#endif

ReferenceCounter::ReferenceCounter(const QString &debugName) :
#ifdef EXTRA_DEBUG
    m_debugName(debugName),
#endif
    m_referenceCount(1)
{
    (void) debugName;
#ifdef LEAK_DEBUG
    QMutexLocker locker(&leakLock);
    leakMap[this] = LeakInfo(debugName);
#endif
}

ReferenceCounter::~ReferenceCounter(void)
{
    if (0 != m_referenceCount)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Object deleted with non-zero reference count!");
    }
#ifdef LEAK_DEBUG
    QMutexLocker locker(&leakLock);
    QMap<ReferenceCounter*,LeakInfo>::iterator it = leakMap.find(this);
    assert(it != leakMap.end());
    leakMap.erase(it);
#endif
}

int ReferenceCounter::IncrRef(void)
{
#ifdef LEAK_DEBUG
    QMutexLocker locker(&leakLock);
#endif

    int val = m_referenceCount.fetchAndAddRelease(1) + 1;

#ifdef EXTRA_DEBUG
    LOG(VB_REFCOUNT, LOG_INFO, QString("%1(0x%2)::IncrRef() -> %3")
        .arg(m_debugName).arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#else
    LOG(VB_REFCOUNT, LOG_INFO, QString("(0x%2)::IncrRef() -> %3")
        .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#endif

#ifdef LEAK_DEBUG
    leakMap[this].refCount = val;
#endif

    return val;
}

int ReferenceCounter::DecrRef(void)
{
#ifdef LEAK_DEBUG
    QMutexLocker locker(&leakLock);
#endif

    int val = m_referenceCount.fetchAndAddRelaxed(-1) - 1;

#ifdef LEAK_DEBUG
    leakMap[this].refCount = val;
    locker.unlock();
#endif

#ifdef EXTRA_DEBUG
    LOG(VB_REFCOUNT, LOG_INFO, QString("%1(0x%2)::DecrRef() -> %3")
        .arg(m_debugName).arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#else
    LOG(VB_REFCOUNT, LOG_INFO, QString("(0x%2)::DecrRef() -> %3")
        .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#endif

#ifdef NO_DELETE_DEBUG
    if (val < 0)
    {
        LOG(VB_REFCOUNT, LOG_ERR, QString("(0x%2)::DecrRef() -> %3 !!!")
            .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
    }
#else
    if (0 == val)
    {
        delete this;
        return val;
    }
#endif

    return val;
}
