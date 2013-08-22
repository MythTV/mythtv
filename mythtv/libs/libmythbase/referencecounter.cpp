// -*- Mode: c++ -*-

#include <QString>

#include "referencecounter.h"
#include "mythlogging.h"

#ifdef EXTRA_DEBUG
// Uncomment this to see missing DecrRefs()
#define LEAK_DEBUG
// Uncoment this to see extra DecrRefs(), no memory will ever be freed..
//#define NO_DELETE_DEBUG
#endif

#ifdef LEAK_DEBUG
#include <QReadWriteLock>
#include <QMap>
#include "../libmythui/mythimage.h"
static QReadWriteLock leakLock;
struct LeakInfo
{
    LeakInfo() : refCount(0) {}
    LeakInfo(const QString &n) : name(n), refCount(1) {}
    QString name;
    QAtomicInt refCount;
};
static QMap<ReferenceCounter*,LeakInfo> leakMap;
void ReferenceCounter::PrintDebug(void)
{
    QReadLocker locker(&leakLock);
    QList<uint64_t> logType;
    QStringList logLines;

    uint cnt = 0;
    QMap<ReferenceCounter*,LeakInfo>::iterator it = leakMap.begin();
    for (; it != leakMap.end(); ++it)
    {
        if ((*it).refCount.fetchAndAddOrdered(0) == 0)
            continue;
        if ((*it).name.startsWith("CommandLineArg"))
            continue;
        if (!it.key()->m_logDebug)
            continue;
        cnt += 1;
    }
    logType += (cnt) ? VB_GENERAL : VB_REFCOUNT;
    logLines += QString("Leaked %1 reference counted objects").arg(cnt);

    for (it = leakMap.begin(); it != leakMap.end(); ++it)
    {
        if ((*it).refCount.fetchAndAddOrdered(0) == 0)
            continue;
        if ((*it).name.startsWith("CommandLineArg"))
            continue;
        if (!it.key()->m_logDebug)
            continue;

        logType += VB_REFCOUNT;
        logLines +=
            QString("  Leaked %1(0x%2) reference count %3")
            .arg((*it).name)
            .arg(reinterpret_cast<intptr_t>(it.key()),0,16)
            .arg((*it).refCount);
    }

    locker.unlock();

    for (uint i = 0; i < (uint)logType.size() && i < (uint)logLines.size(); i++)
        LOG(logType[i], LOG_INFO, logLines[i]);
}
#else
void ReferenceCounter::PrintDebug(void) {}
#endif

ReferenceCounter::ReferenceCounter(const QString &debugName, bool logDebug) :
#ifdef EXTRA_DEBUG
    m_debugName(debugName),
#endif
    m_logDebug(logDebug),
    m_referenceCount(1)
{
    (void) debugName;
#ifdef LEAK_DEBUG
    QWriteLocker locker(&leakLock);
    leakMap[this] = LeakInfo(debugName);
#endif
}

ReferenceCounter::~ReferenceCounter(void)
{
    if (m_referenceCount.fetchAndAddRelaxed(0) > 1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Object deleted with non-zero or one reference count!");
    }
#ifdef LEAK_DEBUG
    QWriteLocker locker(&leakLock);
    leakMap.erase(leakMap.find(this));
#endif
}

int ReferenceCounter::IncrRef(void)
{
    int val = m_referenceCount.fetchAndAddRelease(1) + 1;

    if (m_logDebug)
    {
#ifdef EXTRA_DEBUG
        LOG(VB_REFCOUNT, LOG_INFO, QString("%1(0x%2)::IncrRef() -> %3")
            .arg(m_debugName).arg(reinterpret_cast<intptr_t>(this),0,16)
            .arg(val));
#else
        LOG(VB_REFCOUNT, LOG_INFO, QString("(0x%2)::IncrRef() -> %3")
            .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#endif
    }

#ifdef LEAK_DEBUG
    QReadLocker locker(&leakLock);
    leakMap[this].refCount.fetchAndAddOrdered(1);
#endif

    return val;
}

int ReferenceCounter::DecrRef(void)
{
    int val = m_referenceCount.fetchAndAddRelaxed(-1) - 1;

#ifdef LEAK_DEBUG
    {
        QReadLocker locker(&leakLock);
        leakMap[this].refCount.fetchAndAddOrdered(-1);
    }
#endif

    if (m_logDebug)
    {
#ifdef EXTRA_DEBUG
        LOG(VB_REFCOUNT, LOG_INFO, QString("%1(0x%2)::DecrRef() -> %3")
            .arg(m_debugName).arg(reinterpret_cast<intptr_t>(this),0,16)
            .arg(val));
#else
        LOG(VB_REFCOUNT, LOG_INFO, QString("(0x%2)::DecrRef() -> %3")
            .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#endif
    }

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
