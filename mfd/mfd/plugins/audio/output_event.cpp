/*
	output_event.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for audio output events
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
*/

#include "output_event.h"


OutputEvent::OutputEvent(Type t)
	        :QCustomEvent(t), error_msg(0), elasped_seconds(0), written_bytes(0),
	         brate(0), freq(0), prec(0), chan(0)
{
}

OutputEvent::OutputEvent(long s, unsigned long w, int b, int f, int p, int c)
	        :QCustomEvent(Info), error_msg(0), elasped_seconds(s), written_bytes(w),
             brate(b), freq(f), prec(p), chan(c)
{
}

OutputEvent::OutputEvent(const QString &e)
	        :QCustomEvent(Error), elasped_seconds(0), written_bytes(0),
	         brate(0), freq(0), prec(0), chan(0)
{
    error_msg = new QString(e.utf8());
}


OutputEvent::~OutputEvent()
{
    if (error_msg)
    {
        delete error_msg;
    }
}

const QString* OutputEvent::errorMessage() const
{ 
    return error_msg;
}

const long& OutputEvent::elapsedSeconds() const
{ 
    return elasped_seconds;
}

const unsigned long& OutputEvent::writtenBytes() const
{ 
    return written_bytes;
}

const int& OutputEvent::bitrate() const
{ 
    return brate;
}

const int& OutputEvent::frequency() const
{ 
    return freq;
}

const int& OutputEvent::precision() const
{ 
    return prec;
}

const int& OutputEvent::channels() const
{ 
    return chan;
}


