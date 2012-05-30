// -*- Mode: c++ -*-

#ifndef MYTHREFCOUNT_H_
#define MYTHREFCOUNT_H_

#include <QAtomicInt>
#include "mythbaseexp.h"

/** \brief General purpose reference counter.
 *
 *  This class uses QAtomicInt for lockless reference counting.
 *
 */
class MBASE_PUBLIC ReferenceCounter
{
  public:
    /// Creates reference counter with an initial value of 1.
    ReferenceCounter(void);

    /// Increments reference count.
    void IncrRef(void);

    /// Decrements reference count and deletes on 0.
    /// \return true if object is deleted
    bool DecrRef(void);

  protected:
    /// Called on destruction, will warn if object deleted with
    /// references in place. Set breakpoint here for debugging.
    virtual ~ReferenceCounter(void);

  private:
    QAtomicInt m_referenceCount;
};

/** \brief This decrements the reference on destruction.
 *
 *  This can be used to ensure a reference to an object is decrimented
 *  at the end of the scope containing ReferenceLocker.
 *
 */
class MBASE_PUBLIC ReferenceLocker
{
  public:
    ReferenceLocker(ReferenceCounter *counter) : m_counter(counter) { }
    ~ReferenceLocker()
    {
        if (m_counter)
            m_counter->DecrRef();
    }
  private:
    ReferenceCounter *m_counter;
};

#endif
