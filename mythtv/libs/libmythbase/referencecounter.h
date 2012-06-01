// -*- Mode: c++ -*-

#ifndef MYTHREFCOUNT_H_
#define MYTHREFCOUNT_H_

// Uncomment this line for more useful reference counting debugging
//#define EXTRA_DEBUG

#include <QAtomicInt>

#ifdef EXTRA_DEBUG
#include <QString>
#else
class QString;
#endif

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
    ReferenceCounter(const QString &debugName);

    /// Increments reference count.
    /// \return last reference count
    virtual int IncrRef(void);

    /// Decrements reference count and deletes on 0.
    /// \return last reference count, 0 if deleted
    virtual int DecrRef(void);

  protected:
    /// Called on destruction, will warn if object deleted with
    /// references in place. Set breakpoint here for debugging.
    virtual ~ReferenceCounter(void);

  protected:
#ifdef EXTRA_DEBUG
    const QString m_debugName;
#endif
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
