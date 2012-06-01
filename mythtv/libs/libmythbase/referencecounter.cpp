// -*- Mode: c++ -*-

#include <QString>

#include "referencecounter.h"
#include "mythlogging.h"

ReferenceCounter::ReferenceCounter(const QString &debugName) :
#ifdef EXTRA_DEBUG
    m_debugName(debugName),
#endif
    m_referenceCount(1)
{
    (void) debugName;
}

ReferenceCounter::~ReferenceCounter(void)
{
    if (0 != m_referenceCount)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Object deleted with non-zero reference count!");
    }
}

int ReferenceCounter::IncrRef(void)
{
    int val = m_referenceCount.fetchAndAddRelease(1) + 1;

#ifdef EXTRA_DEBUG
    LOG(VB_REFCOUNT, LOG_DEBUG, QString("%1(0x%2)::IncrRef() -> %3")
        .arg(m_debugName).arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#else
    LOG(VB_REFCOUNT, LOG_DEBUG, QString("(0x%2)::IncrRef() -> %3")
        .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#endif

    return val;
}

int ReferenceCounter::DecrRef(void)
{
    int val = m_referenceCount.fetchAndAddRelaxed(-1) - 1;

#ifdef EXTRA_DEBUG
    LOG(VB_REFCOUNT, LOG_DEBUG, QString("%1(0x%2)::DecrRef() -> %3")
        .arg(m_debugName).arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#else
    LOG(VB_REFCOUNT, LOG_DEBUG, QString("(0x%2)::DecrRef() -> %3")
        .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
#endif

    if (0 == val)
    {
        delete this;
        return val;
    }

    return val;
}
