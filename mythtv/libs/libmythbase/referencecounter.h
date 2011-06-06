// -*- Mode: c++ -*-

#ifndef MYTHREFCOUNT_H_
#define MYTHREFCOUNT_H_

#include <QObject>
#include <QMutex>
#include "mythbaseexp.h"

class MBASE_PUBLIC ReferenceCounter : public QObject
{
    Q_OBJECT
  public:
    ReferenceCounter(void);
   ~ReferenceCounter(void) {};

    virtual void UpRef(void);
    virtual bool DownRef(void);
  private:
    QMutex m_refLock;
    uint m_refCount;
};

class MBASE_PUBLIC ReferenceLocker
{
  public:
    ReferenceLocker(ReferenceCounter *counter, bool upref=true);
   ~ReferenceLocker();
  private:
    ReferenceCounter *m_refObject;
};

#endif
