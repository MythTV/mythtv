// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef __recycler_h
#define __recycler_h

#include <QWaitCondition>
#include <QMutex>

class Buffer;


class Recycler
{
public:
    Recycler(unsigned int sz); // sz = size in bytes
    ~Recycler();

    bool full() const;
    bool empty() const;

    int available() const;
    int used() const;

    Buffer *next(); // get next in queue
    Buffer *get(); // get next in recycle

    void add(); // add to queue
    void done(); // add to recycle

    void clear(); // clear queue

    unsigned int size() const; // size in bytes

    QMutex *mutex() { return &mtx; }
    QWaitCondition *cond() { return &cnd; }


private:
    unsigned int buffer_count, add_index, done_index, current_count;
    Buffer **buffers;
    QMutex mtx;
    QWaitCondition cnd;
};

#endif // __recycler_h
