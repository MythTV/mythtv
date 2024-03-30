//
//  referencecounterlist.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 16/08/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MYTHTV_REFERENCECOUNTERLIST_H
#define MYTHTV_REFERENCECOUNTERLIST_H

#include "mythbaseexp.h"
#include "referencecounter.h"
#include <QList>

template <class T>
class RefCountHandler
{
public:
    RefCountHandler() : r(new T()) { }
    RefCountHandler(T *_r) : r(_r) { r->IncrRef(); }
    RefCountHandler(const RefCountHandler &other) : r(other.r) { r->IncrRef(); }
    // The first two lines of this function are the clang-tidy
    // recommended solution to make a function properly handle
    // self-assignment.  No idea why clang-tidy doesn't recognize
    // this, unless it has something to do with templates?
    //
    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
    RefCountHandler &operator= (const RefCountHandler &other)
    {
        if (this == &other)
            return *this;
        other.r->IncrRef();
        r->DecrRef();
        r = other.r;
        return *this;
    }
    ~RefCountHandler() { r->DecrRef(); }
    operator T *() const { return r; } // Convert to T* automatically.
    T *operator*() { return r; }
    const T* operator*() const { return r; }
    T *operator->() { return r; }
    const T *operator->() const { return r; }

private:
    T *r; //NOLINT(readability-identifier-naming)
};

/** \brief General purpose reference counted list.
 * T typename must be a ReferenceCounter inheriting object
 * provide the same access methods as a QList
 */
template <class T>

class RefCountedList : public QList<RefCountHandler<T> >
{
public:
    /**
     * Removes the item at index position i and returns it.
     * i must be a valid index position in the list (i.e., 0 <= i < size()).
     * takeAt takes ownership of the object from the list.
     * (e.g if the object only exists in the list, it won't be deleted)
     */
    T *takeAt(int i)
    {
        RefCountHandler<T> rch = QList<RefCountHandler<T> >::takeAt(i);
        rch->IncrRef();
        return rch;
    }
    /**
     * Removes the first item in the list and returns it.
     * This is the same as takeAt(0).
     * This function assumes the list is not empty.
     * To avoid failure, call isEmpty() before calling this function.
     * takeFirst takes ownership of the object from the list.
     * (e.g if the object only exists in the list, it won't be deleted)
     */
    T *takeFirst(void)
    {
        RefCountHandler<T> rch = QList<RefCountHandler<T> >::takeFirst();
        rch->IncrRef();
        return rch;
    }
    /**
     * Removes the last item in the list and returns it.
     * This is the same as takeAt(size() - 1).
     * This function assumes the list is not empty.
     * To avoid failure, call isEmpty() before calling this function.
     * takeFirst takes ownership of the object from the list.
     * (e.g if the object only exists in the list, it won't be deleted)
     */
    T *takeLast(void)
    {
        RefCountHandler<T> rch = QList<RefCountHandler<T> >::takeLast();
        rch->IncrRef();
        return rch;
    }
    /**
     * Removes the item at index position i and returns it.
     * i must be a valid index position in the list (i.e., 0 <= i < size()).
     */
    RefCountHandler<T> takeAtAndDecr(int i)
    {
        return QList<RefCountHandler<T> >::takeAt(i);
    }
    /**
     * Removes the first item in the list and returns it.
     * This is the same as takeAtAndDecr(0).
     * This function assumes the list is not empty.
     * To avoid failure, call isEmpty() before calling this function.
     */
    RefCountHandler<T> takeFirstAndDecr(void)
    {
        return QList<RefCountHandler<T> >::takeFirst();
    }
    /**
     * Removes the last item in the list and returns it.
     * This is the same as takeAtAndDecr(size() - 1).
     * This function assumes the list is not empty.
     * To avoid failure, call isEmpty() before calling this function.
     */
    RefCountHandler<T> takeLastAndDecr(void)
    {
        return QList<RefCountHandler<T> >::takeLast();
    }
    RefCountedList<T> mid(int pos, int length = -1) const
    {
        if (length < 0 || pos + length > this->size())
        {
            length = this->size() - pos;
        }
        if (pos == 0 && length == this->size())
        {
            return *this;
        }
        RefCountedList<T> cpy;
        if (length <= 0)
        {
            return cpy;
        }
        for (int i = pos; i < length; i++)
        {
            cpy.append(this->at(pos));
        }
        return cpy;
    }
    RefCountedList<T> operator+(const RefCountedList<T> &other) const
    {
        RefCountedList<T> list = *this;
        list += other;
        return list;
    }
    RefCountedList<T> &operator+=(const RefCountedList<T> &other)
    {
        QList<RefCountHandler<T> >::operator+=(other);
        return *this;
    }
    RefCountedList<T> &operator+=(const RefCountHandler<T> &value)
    {
        QList<RefCountHandler<T> >::operator+=(value);
        return *this;
    }
    RefCountedList<T> &operator<<(const RefCountedList<T> &other)
    {
        QList<RefCountHandler<T> >::operator<<(other);
        return *this;
    }
    RefCountedList<T> &operator<<(const RefCountHandler<T> &value)
    {
        QList<RefCountHandler<T> >::operator<<(value);
        return *this;
    }
    // The first two lines of this function are the clang-tidy
    // recommended solution to make a function properly handle
    // self-assignment.  No idea why clang-tidy doesn't recognize
    // this, unless it has something to do with templates?
    //
    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
    RefCountedList<T> &operator=(const RefCountedList<T> &other)
    {
        if (this == &other)
            return *this;
        QList<RefCountHandler<T> >::operator=(other);
        return *this;
    }

    RefCountedList() = default;
    RefCountedList(const RefCountedList&) = default;
};

using ReferenceCounterList = RefCountedList<ReferenceCounter>;

#endif /* defined(MYTHTV_REFERENCECOUNTERLIST_H) */
