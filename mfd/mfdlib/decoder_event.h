#ifndef DECODER_EVENT_H_
#define DECODER_EVENT_H_
/*
	decoder_event.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Events the decoder object can toss
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include <qevent.h>

class DecoderEvent : public QCustomEvent
{

  public:

    enum Type 
    { 
        Decoding = (QEvent::User + 100), 
        Stopped, 
        Finished, 
        Error 
    };

    DecoderEvent(Type t);
    DecoderEvent(QString *e);
    ~DecoderEvent();

    const QString *errorMessage();

  private:

    QString *error_msg;
};


#endif
