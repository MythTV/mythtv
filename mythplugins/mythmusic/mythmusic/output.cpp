// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include <qobject.h>

#include "output.h"
#include "visual.h"

#include <stdio.h>


Output::Output(unsigned int sz)
    : r(sz), bufsize(sz)
{
}


Output::~Output()
{
}


void Output::addListener(QObject *o)
{
    if (listeners.find(o) == -1)
	listeners.append(o);
}


void Output::removeListener(QObject *o)
{
    listeners.remove(o);
}


void Output::dispatch(OutputEvent &e)
{
    QObject *object = listeners.first();
    while (object) {
	QThread::postEvent(object, new OutputEvent(e));
	object = listeners.next();
    }
}


void Output::error(const QString &e) {
    QObject *object = listeners.first();
    while (object) {
	QThread::postEvent(object, new OutputEvent(e));
	object = listeners.next();
    }
}

void Output::addVisual(Visual *v)
{
    if (visuals.find(v) == -1) {
       visuals.append(v);
    }
}

void Output::removeVisual(Visual *v)
{
    visuals.remove(v);
}

void Output::dispatchVisual(Buffer *buffer, unsigned long written,
                            int chan, int prec)
{
    if (! buffer)
       return;

    Visual *visual = visuals.first();
    while (visual) {
       visual->mutex()->lock();
       visual->add(buffer, written, chan, prec);
       visual->mutex()->unlock();

       visual = visuals.next();
    }
}

void Output::prepareVisuals()
{
    Visual *visual = visuals.first();
    while (visual) {
       visual->mutex()->lock();
       visual->prepare();
       visual->mutex()->unlock();

       visual = visuals.next();
    }
}
