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

    uint cnt = 0;
    QMap<ReferenceCounter*,LeakInfo>::iterator it = leakMap.begin();
    for (; it != leakMap.end(); ++it)
        cnt += (*it).name.startsWith("CommandLineArg") ? 0 : 1;

    LOG((cnt) ? VB_GENERAL : VB_REFCOUNT, LOG_INFO,
        QString("Leaked %1 reference counted objects").arg(cnt));

    for (it = leakMap.begin(); it != leakMap.end(); ++it)
    {
        if ((*it).name.startsWith("CommandLineArg"))
            continue;
        LOG(VB_REFCOUNT, LOG_INFO,
            QString("  Leaked %1(0x%2) reference count %3")
            .arg((*it).name)
            .arg(reinterpret_cast<intptr_t>(it.key()),0,16)
            .arg((*it).refCount));
/// This part will not work on some non-Linux machines..
#if defined(__linux__) || defined(__LINUX__)
        MythImage *img = NULL;
        if ((*it).name.startsWith("MythImage") ||
            (*it).name.startsWith("MythQtImage"))
        {
            img = reinterpret_cast<MythImage*>(it.key());
        }
        if (img)
        {
            LOG(VB_REFCOUNT, LOG_INFO,
                QString("  Image id %1 cache size %2 file '%3'")
                .arg(img->GetID())
                .arg(img->GetCacheSize())
                .arg(img->GetFileName()));
        }
#endif
///
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
    QWriteLocker locker(&leakLock);
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
    QWriteLocker locker(&leakLock);
    leakMap.erase(leakMap.find(this));
#endif
}

int ReferenceCounter::IncrRef(void)
{
    int val = m_referenceCount.fetchAndAddRelease(1) + 1;

#ifdef EXTRA_DEBUG
    LOG(VB_REFCOUNT, LOG_INFO, QString("%1(0x%2)::IncrRef() -> %3")
        .arg(m_debugName).arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#else
    LOG(VB_REFCOUNT, LOG_INFO, QString("(0x%2)::IncrRef() -> %3")
        .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#endif

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
