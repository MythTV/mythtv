#ifndef RECYCLER_H_
#define RECYCLER_H_
/*
	recycler.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for recycler
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
*/

#include <qmutex.h>
#include <qwaitcondition.h>

#include "buffer.h"

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

#endif // recycler_h_
