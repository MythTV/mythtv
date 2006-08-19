#ifndef QUICKSP_H_
#define QUICKSP_H_

struct NoLock
{
    void lock() {}
    void unlock() {}
};

// TODO: implement for threads
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
    simple_ref_ptr() : m_ref(NULL)
    {
    }

    simple_ref_ptr(T *ptr)
    {
        m_ref = new ref(ptr);
    }

    simple_ref_ptr(const simple_ref_ptr &rhs) : m_ref(NULL)
    {
        *this = rhs;
    }

    ~simple_ref_ptr()
    {
        unref();
    }

    simple_ref_ptr &operator=(const simple_ref_ptr &rhs)
    {
        rhs.m_ref->inc();
        unref();
        m_ref = rhs.m_ref;

        return *this;
    }

    T *operator->()
    {
        return get();
    }

    T *operator->() const
    {
        return get();
    }

    T &operator*()
    {
        return *get();
    }

    T &operator*() const
    {
        return *get();
    }

    T *get() const
    {
        if (m_ref) return m_ref->get();

        return NULL;
    }

    void reset(T *ptr)
    {
        unref();
        m_ref = new ref(ptr);
    }

  private:
    class ref : public Locker
    {
      public:
        ref(T *ptr) : m_count(1), m_type(ptr) {}

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
        unsigned int m_count;
        T *m_type;
    };

    void unref()
    {
        if (m_ref && m_ref->dec() <= 0)
        {
            delete m_ref;
            m_ref = NULL;
        }
    }

  private:
    ref *m_ref;
};

#endif // QUICKSP_H_
