#ifndef _AUTO_DELETE_DEQUE_H_
#define _AUTO_DELETE_DEQUE_H_

// C++ headers
#include <deque>
#include <algorithm>

template<typename T>
class AutoDeleteDeque
{
  public:
    AutoDeleteDeque(bool auto_delete = true) : autodelete(auto_delete) {}
    ~AutoDeleteDeque() { clear(); }

    typedef typename std::deque< T > List;
    typedef typename List::iterator iterator;
    typedef typename List::const_iterator const_iterator;
    typedef typename List::reverse_iterator reverse_iterator;
    typedef typename List::const_reverse_iterator const_reverse_iterator;

    T operator[](uint index)
    {
        if (index < list.size())
            return list[index];
        return NULL;
    }
    const T operator[](uint index) const
    {
        if (index < list.size())
            return list[index];
        return NULL;
    }

    T take(uint i);
    iterator erase(iterator it)
    {
        if (autodelete)
            delete *it;
        return list.erase(it);
    }
    void clear(void)
    {
        while (autodelete && !list.empty())
        {
            delete list.back();
            list.pop_back();
        }
        list.clear();
    }

    iterator begin(void)             { return list.begin(); }
    iterator end(void)               { return list.end();   }
    const_iterator begin(void) const { return list.begin(); }
    const_iterator end(void)   const { return list.end();   }
    reverse_iterator rbegin(void)             { return list.rbegin(); }
    reverse_iterator rend(void)               { return list.rend();   }
    const_reverse_iterator rbegin(void) const { return list.rbegin(); }
    const_reverse_iterator rend(void)   const { return list.rend();   }

    T back(void)                     { return list.back();  }
    const T back(void)         const { return list.back();  }

    bool empty(void)  const { return list.empty(); }
    size_t size(void) const { return list.size(); }
    void push_front(T info) { list.push_front(info); }
    void push_back( T info) { list.push_back( info); }

    // compatibility with old Q3PtrList
    void setAutoDelete(bool auto_delete) { autodelete = auto_delete; }

  protected:
    List list;
    bool autodelete;
};

template<typename T>
T AutoDeleteDeque<T>::take(uint index)
{
    iterator it = list.begin();
    for (uint i = 0; i < index; i++, it++)
    {
        if (it == list.end())
            return NULL;
    }
    T info = *it;
    list.erase(it);
    return info;
}

#endif // _AUTO_DELETE_DEQUE_H_
