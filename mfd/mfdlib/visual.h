#ifndef VISUAL_H_
#define VISUAL_H_
/*
	visual.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers and methods for a largely virtual visual(-ization) class
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include <qmutex.h>

class Buffer;
class Decoder;
class Output;

class Visual
{

  public:

    Visual() : dec(0), out(0) { ; }
    virtual ~Visual() { ; }

    virtual void add(Buffer *, unsigned long, int, int) = 0;
    virtual void prepare() = 0;

    Decoder *decoder() const { return dec; }
    void setDecoder(Decoder *d) { dec = d; }

    Output *output() const { return out; }
    void setOutput(Output *o) { out = o; }

    QMutex *mutex() { return &mtx; }


private:

    Decoder *dec;
    Output *out;
    QMutex mtx;
};


#endif // visual_h_
