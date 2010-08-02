// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef __visual_h
#define __visual_h

#include <QMutex>

class Decoder;
class AudioOutput;
namespace MythTV
{
class Visual
{
public:
    Visual() { ; }
    virtual ~Visual() { ; }

    virtual void add(uchar *b, unsigned long b_len, unsigned long w, int c, int p) = 0;
    virtual void prepare() = 0;

    QMutex *mutex() { return &mtx; }


private:
    QMutex mtx;
};
};

#endif // __visual_h
