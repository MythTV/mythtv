#ifndef QUICKSP_H_
#define QUICKSP_H_

#include "mythmetaexp.h"

struct NoLock
{
    void lock() {}
    void unlock() {}
};

// TODO: implement for threads
// If implemented for threads move the instance pointer to simple_ref_ptr
// so simple access isn't synchronized.
struct ThreadLock
{
    void lock() {}

    void unlock() {}
};

// TODO: Get a real reference counted smart pointer in libmyth
template <typename T, class Locker = NoLock>
class simple_ref_ptr
{
  public:
    simple_ref_ptr() : m_ref(nullptr)
    {
    }

    simple_ref_ptr(T *ptr)
      : m_ref(new ref(ptr))
    {
    }

    simple_ref_ptr(const simple_ref_ptr &rhs) : m_ref(nullptr)
    {
        *this = rhs;
    }

    ~simple_ref_ptr()
    {
        unref();
    }

    // The first two lines of this function are the clang-tidy
    // recommended solution to make a function properly handle
    // self-assignment.  No idea why clang-tidy doesn't recognize
    // this, unless it has something to do with templates?
    //
    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
    simple_ref_ptr &operator=(const simple_ref_ptr &rhs)
    {
        if (this == &rhs)
            return *this;
        rhs.m_ref->inc();
        unref();
        m_ref = rhs.m_ref;

        return *this;
    }

    T *operator->() const
    {
        return get();
    }

    T &operator*() const
    {
        return *get();
    }

    T *get() const
    {
        if (m_ref) return m_ref->get();

        return nullptr;
    }

    void reset(T *ptr)
    {
        unref();
        m_ref = new ref(ptr);
    }

    using fake_bool = T *(simple_ref_ptr<T>::*)() const;

    operator fake_bool() const
    {
        return m_ref == nullptr ? nullptr : &simple_ref_ptr<T>::get;
    }

    bool operator!() const
    {
        return m_ref == nullptr;
    }

  private:
    class ref : public Locker
    {
      public:
        explicit ref(T *ptr) : m_type(ptr) {}

        ~ref()
        {
            delete m_type;
        }

        unsigned int inc()
        {
            this->lock();
            ++m_count;
            this->unlock();
            return m_count;
        }

        unsigned int dec()
        {
            this->lock();
            --m_count;
            this->unlock();
            return m_count;
        }

        T *get()
        {
            return m_type;
        }

        T *get() const
        {
            return m_type;
        }

      private:
        unsigned int m_count {1};
        T *m_type;
    };

    void unref()
    {
        if (m_ref && m_ref->dec() <= 0)
        {
            delete m_ref;
            m_ref = nullptr;
        }
    }

  private:
    ref *m_ref;
};

template <typename T>
bool operator==(const simple_ref_ptr<T> &lhs, const simple_ref_ptr<T> &rhs)
{
    return lhs.get() == rhs.get();
}

template <typename T>
bool operator!=(const simple_ref_ptr<T> &lhs, const simple_ref_ptr<T> &rhs)
{
    return lhs.get() != rhs.get();
}

#endif // QUICKSP_H_
