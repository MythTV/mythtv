// -*- Mode: c++ -*-

#ifndef MYTH_DEQUE_H
#define MYTH_DEQUE_H

#include <QString>
#include <QStringList>
#include <deque>
using std::deque;

template<typename T>
inline T myth_deque_init(const T */*unused*/) { return nullptr; }
template<>
inline int myth_deque_init(const int * /*unused*/) { return 0; }
template<>
inline uint myth_deque_init(const uint * /*unused*/) { return 0; }
template<>
inline QString myth_deque_init(const QString * /*unused*/) { return QString(); }
template<>
inline QStringList myth_deque_init(const QStringList * /*unused*/) { return QStringList(); }

/** \class MythDeque
 *  \brief MythDeque is similar to QPtrQueue, while being based off
 *         deque, this allows that items that are not at the head of
 *         the queue can be seen/deleted.
 */
template<typename T>
class MythDeque : public deque<T>
{
  public:
    /// \brief Removes item from front of list and returns a copy. O(1).
    T dequeue()
    {
        T *dummy = nullptr;
        if (deque<T>::empty())
            return myth_deque_init(dummy);
        T item = deque<T>::front();
        deque<T>::pop_front();
        return item;
    }
    /// \brief Adds item to the back of the list. O(1).
    void enqueue(T d) { deque<T>::push_back(d); }

    using iterator = typename deque<T>::iterator;
    using const_iterator = typename deque<T>::const_iterator;
    using size_type = typename deque<T>::size_type;

    /// \brief Finds an item in the list via linear search O(n).
    iterator find(T const item)
    {
        for (auto it = deque<T>::begin(); it != deque<T>::end(); ++it)
            if (*it == item)
                return it;
        return deque<T>::end();
    }

    /// \brief Finds an item in the list via linear search O(n).
    const_iterator find(T const item) const
    {
        for (auto it = deque<T>::begin(); it != deque<T>::end(); ++it)
            if (*it == item)
                return it;
        return deque<T>::end();
    }

    /// \brief Removes any item from list. O(n).
    void remove(T const item)
    {
        auto it = find(item);
        if (it != deque<T>::end())
            deque<T>::erase(it);
    }

    /// \brief Returns true if item is in list. O(n).
    // cppcheck-suppress constParameter
    bool contains(T const &item) const
        { return find(item) != deque<T>::end(); }

    /// \brief Returns size of list. O(1).
    size_type count() const { return deque<T>::size(); }

    /// \brief Returns item at head of list. O(1).
    T head()
        { if (!deque<T>::empty()) return deque<T>::front();
          T *dummy = nullptr; return myth_deque_init(dummy); }
    /// \brief Returns item at head of list. O(1).
    T head() const
        { if (!deque<T>::empty()) return deque<T>::front();
          T *dummy = NULL; return myth_deque_init(dummy); }

    /// \brief Returns item at tail of list. O(1).
    T tail()
        { if (!deque<T>::empty()) return deque<T>::back();
          T *dummy = nullptr; return myth_deque_init(dummy); }
    /// \brief Returns item at tail of list. O(1).
    T tail() const
        { if (!deque<T>::empty()) return deque<T>::back();
          T *dummy = NULL; return myth_deque_init(dummy); }
};

#endif // MYTH_DEQUE_H
