#ifndef OUTPUT_H_
#define OUTPUT_H_
/*
	output.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for output
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include <qthread.h>
#include <qptrlist.h>
#include <qobject.h>

#include "recycler.h"
#include "visual.h"
#include "output_event.h"

class AudioPlugin;

class Output : public QThread
{

  public:

    Output(unsigned int, AudioPlugin *owner);
    virtual ~Output();

    Recycler *recycler() { return &r; }

    void addListener(QObject *);
    void removeListener(QObject *);

    void addVisual(Visual *);
    void removeVisual(Visual *);
    
    QMutex *mutex() { return &mtx; }

    void setBufferSize(unsigned int sz) { bufsize = sz; }
    unsigned int bufferSize() const { return bufsize; }

    // abstract

    virtual bool initialized() const = 0;
    virtual bool initialize() = 0;
    virtual void configure(long, int, int, int) = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual long written() = 0;
    virtual long latency() = 0;
    virtual void seek(long) = 0;
    virtual void resetTime() = 0;
    virtual bool isPaused() = 0;

protected:

    void dispatch(OutputEvent &e);
    void error(const QString &e);
    void dispatchVisual(Buffer *, unsigned long, int, int);
    void prepareVisuals();

private:

    QMutex mtx;
    Recycler r;
    QPtrList<QObject> listeners;
    QPtrList<Visual> visuals;
    
    unsigned int bufsize;

    AudioPlugin *parent;
};


#endif // __output_h
