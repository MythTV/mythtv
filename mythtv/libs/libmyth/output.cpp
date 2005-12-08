// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include <qobject.h>
#include <qapplication.h>

#include "output.h"
#include "visual.h"

OutputListeners::OutputListeners()
{
    bufsize=0;
}


OutputListeners::~OutputListeners()
{
}


void OutputListeners::error(const QString &e) {
    QObject *object = firstListener();
    while (object) {
	QApplication::postEvent(object, new OutputEvent(e));
	object = nextListener();
    }
}

void OutputListeners::addVisual(MythTV::Visual *v)
{
    if (visuals.find(v) == -1) {
       visuals.append(v);
    }
}

void OutputListeners::removeVisual(MythTV::Visual *v)
{
    visuals.remove(v);
}

void OutputListeners::dispatchVisual(uchar *buffer, unsigned long b_len,
                        unsigned long written, int chan, int prec)
{
    if (! buffer)
       return;

    MythTV::Visual *visual = visuals.first();
    while (visual) {
       visual->mutex()->lock();
       visual->add(buffer, b_len, written, chan, prec);
       visual->mutex()->unlock();

       visual = visuals.next();
    }
}

void OutputListeners::prepareVisuals()
{
    MythTV::Visual *visual = visuals.first();
    while (visual) {
       visual->mutex()->lock();
       visual->prepare();
       visual->mutex()->unlock();

       visual = visuals.next();
    }
}
