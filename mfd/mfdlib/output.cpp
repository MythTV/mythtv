/*
	output.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	output object
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
*/

#include <stdio.h>

#include <qapplication.h>
#include <qobject.h>

#include "output.h"
#include "visual.h"

Output::Output(unsigned int sz)
    : r(sz), bufsize(sz)
{
}


Output::~Output()
{
}

void Output::addListener(QObject *o)
{
    //
    //  Add this listener to my list, as long as it's not already there
    //
    
    if (listeners.find(o) == -1)
    {
    	listeners.append(o);
    }
}


void Output::removeListener(QObject *o)
{
    //
    //  Bye bye listener
    //

    listeners.remove(o);
}


void Output::dispatch(OutputEvent &e)
{
    //
    //  Send the event to anyone who is listening
    //
    
/*
    parent->swallowOutputUpdate(
                                e.elapsedSeconds(),
                                e.channels(),
                                e.bitrate(),
                                e.frequency()
                               );
*/
    
    QObject *object = listeners.first();
    while (object)
    {
	    QApplication::postEvent(object, new OutputEvent(e));
	    object = listeners.next();
    }
}


void Output::error(const QString &e)
{
    //
    //  As with dispatch (above)
    //

    QObject *object = listeners.first();
    while (object) 
    {
	    QApplication::postEvent(object, new OutputEvent(e));
	    object = listeners.next();
    }
}

void Output::addVisual(Visual *v)
{
    //
    //  Add some visual to my list
    //

    if (visuals.find(v) == -1)
    {
       visuals.append(v);
    }
}

void Output::removeVisual(Visual *v)
{
    visuals.remove(v);
}

void Output::dispatchVisual(
                            Buffer *buffer, 
                            unsigned long written,
                            int chan, 
                            int prec
                           )
{
    if (! buffer)
    {
        //
        //  Nothing to send.
        //

       return;
    }

    Visual *visual = visuals.first();
    while (visual)
    {
        //
        //  Lock the visual, then send it data
        //

        visual->mutex()->lock();
        visual->add(buffer, written, chan, prec);
        visual->mutex()->unlock();

        visual = visuals.next();
    }
}

void Output::prepareVisuals()
{
    //
    //  Tell the visuals to get ready
    //

    Visual *visual = visuals.first();
    while (visual)
    {
        //
        //  Lock, prepare, unlock
        //

        visual->mutex()->lock();
        visual->prepare();
        visual->mutex()->unlock();

        visual = visuals.next();
    }
}
