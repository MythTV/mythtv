// -*- Mode: c++ -*-

#include <QString>
#include "referencecounter.h"
#include "mythlogging.h"

ReferenceCounter::ReferenceCounter(void) : m_referenceCount(1)
{
}

ReferenceCounter::~ReferenceCounter(void)
{
    if (0 != m_referenceCount)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Object deleted with non-zero reference count!");
    }
}

void ReferenceCounter::IncrRef(void)
{
    int val = m_referenceCount.fetchAndAddRelease(1);

    LOG(VB_REFCOUNT, LOG_DEBUG, QString("(0x%2)::IncrRef() -> %3")
        .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));
}

bool ReferenceCounter::DecrRef(void)
{
    int val = m_referenceCount.fetchAndAddRelaxed(-1);

    LOG(VB_REFCOUNT, LOG_DEBUG, QString("(0x%2)::DecrRef() -> %3")
        .arg(reinterpret_cast<intptr_t>(this),0,16).arg(val));

    if (0 == val)
    {
        delete this;
        return true;
    }

    return false;
}
