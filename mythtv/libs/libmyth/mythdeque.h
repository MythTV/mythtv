#ifndef __MYTH_DEQUE_H__
#define __MYTH_DEQUE_H__

#include <deque>
using namespace std;

// MythDeque is similar to QPtrQueue, while being based off
// deque, so that items that are not at the head of the
// queue can be seen/deleted.

template<typename T>
class MythDeque : public deque<T>
{
  public:
    T dequeue() {
        if (empty())
            return NULL;
        T item = front();
        pop_front();
        return item;
    }
    void enqueue(T d) { push_back(d); }

    typedef typename deque<T>::iterator iterator;
    typedef typename deque<T>::const_iterator const_iterator;
    typedef typename deque<T>::size_type size_type;

    iterator find(T const item)
    {
        for (iterator it = begin(); it != end(); ++it)
            if (*it == item)
                return it;
        return end();
    }

    const_iterator find(T const item) const
    {
        for (const_iterator it = begin(); it != end(); ++it)
            if (*it == item)
                return it;
        return end();
    }
    
    void remove(T const item)
    {
        iterator it = find(item);
        if (it != end())
            erase(it);
    }

    bool contains(T const item) const { return find(item) != end(); }

    size_type count() const { return size(); }

    T head() { return (size()) ? deque<T>::front() : NULL; }
    const T head() const { return (size()) ? deque<T>::front() : NULL; }

    T tail() { return (size()) ? deque<T>::back() : NULL; }
    const T tail() const { return (size()) ? deque<T>::back() : NULL; }
};

#endif // __MYTH_DEQUE_H__
