#ifndef OUTPUT_EVENT_H_
#define OUTPUT_EVENT_H_
/*
	output_event.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for audio output events
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
*/

#include <qevent.h>

class OutputEvent : public QCustomEvent
{
    //
    //  This is an event that the Output object can send. Just lets the
    //  receiver know what is going on.
    // 

public:

    enum Type 
    { 
        Playing = (User + 200), 
        Buffering, 
        Info, 
        Paused,
		Stopped, 
		Error 
    };

    OutputEvent(Type t);
    OutputEvent(long s, unsigned long w, int b, int f, int p, int c);
    OutputEvent(const QString &e);
    ~OutputEvent();


    const QString *errorMessage() const;
    const long &elapsedSeconds() const;
    const unsigned long &writtenBytes() const;
    const int &bitrate() const;
    const int &frequency() const;
    const int &precision() const;
    const int &channels() const;


private:

    QString       *error_msg;
    long          elasped_seconds;
    unsigned long written_bytes;
    int           brate, freq, prec, chan;
};


#endif // __output_event_h
