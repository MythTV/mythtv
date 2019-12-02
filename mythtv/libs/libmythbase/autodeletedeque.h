#ifndef _AUTO_DELETE_DEQUE_H_
#define _AUTO_DELETE_DEQUE_H_

// C++ headers
#include <deque>

template<typename T>
class AutoDeleteDeque
{
  public:
    explicit AutoDeleteDeque(bool auto_delete = true) : m_autodelete(auto_delete) {}
    ~AutoDeleteDeque() { clear(); }

    using List = typename std::deque< T >;
    using iterator = typename List::iterator;
    using const_iterator = typename List::const_iterator;
    using reverse_iterator = typename List::reverse_iterator;
    using const_reverse_iterator = typename List::const_reverse_iterator;

    T operator[](uint index)
    {
        if (index < m_list.size())
            return m_list[index];
        return nullptr;
    }
    const T operator[](uint index) const
    {
        if (index < m_list.size())
            return m_list[index];
        return nullptr;
    }

    T take(uint i);
    iterator erase(iterator it)
    {
        if (m_autodelete)
            delete *it;
        return m_list.erase(it);
    }
    void clear(void)
    {
        while (m_autodelete && !m_list.empty())
        {
            delete m_list.back();
            m_list.pop_back();
        }
        m_list.clear();
    }

    iterator begin(void)             { return m_list.begin(); }
    iterator end(void)               { return m_list.end();   }
    const_iterator begin(void) const { return m_list.begin(); }
    const_iterator end(void)   const { return m_list.end();   }
    const_iterator cbegin(void) const { return m_list.begin(); }
    const_iterator cend(void)   const { return m_list.end();   }
    reverse_iterator rbegin(void)             { return m_list.rbegin(); }
    reverse_iterator rend(void)               { return m_list.rend();   }
    const_reverse_iterator rbegin(void) const { return m_list.rbegin(); }
    const_reverse_iterator rend(void)   const { return m_list.rend();   }
    const_reverse_iterator crbegin(void) const { return m_list.rbegin(); }
    const_reverse_iterator crend(void)   const { return m_list.rend();   }

    T back(void)                     { return m_list.back();  }
    const T back(void)         const { return m_list.back();  }

    bool empty(void)  const { return m_list.empty(); }
    size_t size(void) const { return m_list.size(); }
    void push_front(T info) { m_list.push_front(info); }
    void push_back( T info) { m_list.push_back( info); }

    // compatibility with old Q3PtrList
    void setAutoDelete(bool auto_delete) { m_autodelete = auto_delete; }

  protected:
    List m_list;
    bool m_autodelete;
};

template<typename T>
T AutoDeleteDeque<T>::take(uint index)
{
    iterator it = m_list.begin();
    for (uint i = 0; i < index; i++, ++it)
    {
        if (it == m_list.end())
            return nullptr;
    }
    T info = *it;
    m_list.erase(it);
    return info;
}

#endif // _AUTO_DELETE_DEQUE_H_
