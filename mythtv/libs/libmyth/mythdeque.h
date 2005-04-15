// -*- Mode: c++ -*-

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
    T dequeue()
    {
        if (deque<T>::empty())
            return NULL;
        T item = deque<T>::front();
        deque<T>::pop_front();
        return item;
    }
    void enqueue(T d) { deque<T>::push_back(d); }

    typedef typename deque<T>::iterator iterator;
    typedef typename deque<T>::const_iterator const_iterator;
    typedef typename deque<T>::size_type size_type;

    iterator find(T const item)
    {
        for (iterator it = deque<T>::begin(); it != deque<T>::end(); ++it)
            if (*it == item)
                return it;
        return deque<T>::end();
    }

    const_iterator find(T const item) const
    {
        for (const_iterator it = deque<T>::begin(); it != deque<T>::end(); ++it)
            if (*it == item)
                return it;
        return deque<T>::end();
    }
    
    void remove(T const item)
    {
        iterator it = find(item);
        if (it != deque<T>::end())
            deque<T>::erase(it);
    }

    bool contains(T const item) const
        { return find(item) != deque<T>::end(); }

    size_type count() const { return deque<T>::size(); }

    T head() { return (deque<T>::size()) ? deque<T>::front() : NULL; }
    const T head() const
        { return (deque<T>::size()) ? deque<T>::front() : NULL; }

    T tail() { return (deque<T>::size()) ? deque<T>::back() : NULL; }
    const T tail() const
        { return (deque<T>::size()) ? deque<T>::back() : NULL; }
};

#endif // __MYTH_DEQUE_H__
