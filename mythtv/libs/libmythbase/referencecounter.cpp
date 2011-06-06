// -*- Mode: c++ -*-

#include <QString>
#include <QMutexLocker>
#include "referencecounter.h"
#include "mythverbose.h"

ReferenceCounter::ReferenceCounter(void) :
    m_refCount(1)
{
}

void ReferenceCounter::UpRef(void)
{
    QMutexLocker mlock(&m_refLock);
    m_refCount++;
    VERBOSE(VB_GENERAL|VB_EXTRA, QString("%1(%2)::UpRef() -> %3")
                    .arg(metaObject()->className())
//                    .arg(QString::number((uint)this))
                    .arg("0")
                    .arg(QString::number(m_refCount)));
}

bool ReferenceCounter::DownRef(void)
{
    QMutexLocker mlock(&m_refLock);
    m_refCount--;
    VERBOSE(VB_GENERAL|VB_EXTRA, QString("%1(%2)::DownRef() -> %3")
                    .arg(metaObject()->className())
//                    .arg(QString::number((uint)this))
                    .arg("0")
                    .arg(QString::number(m_refCount)));

    if (m_refCount == 0)
    {
        delete this;
        return true;
    }

    return false;
}

ReferenceLocker::ReferenceLocker(ReferenceCounter *counter, bool upref) :
    m_refObject(counter)
{
    if (upref)
        m_refObject->UpRef();
}

ReferenceLocker::~ReferenceLocker(void)
{
    m_refObject->DownRef();
    m_refObject = NULL;
}
