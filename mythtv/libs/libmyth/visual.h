// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef __visual_h
#define __visual_h

#include <qmutex.h>

class Decoder;
class AudioOutput;
namespace MythTV
{
class Visual
{
public:
    Visual() : dec(0), out(0) { ; }
    virtual ~Visual() { ; }

    virtual void add(uchar *b, unsigned long b_len, unsigned long w, int c, int p) = 0;
    virtual void prepare() = 0;

    Decoder *decoder() const { return dec; }
    void setDecoder(Decoder *d) { dec = d; }

    AudioOutput *output() const { return out; }
    void setOutput(AudioOutput *o) { out = o; }

    QMutex *mutex() { return &mtx; }


private:
    Decoder *dec;
    AudioOutput *out;
    QMutex mtx;
};
};

#endif // __visual_h
