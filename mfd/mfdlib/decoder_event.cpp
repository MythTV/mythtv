/*
	decoder_event.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	decoder events
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "decoder_event.h"

DecoderEvent::DecoderEvent(Type t)
             :QCustomEvent(t), error_msg(0)
{
}

DecoderEvent::DecoderEvent(QString *e)
             :QCustomEvent(Error), error_msg(e)
{
}

DecoderEvent::~DecoderEvent()
{
    if (error_msg)
    {
        delete error_msg;
    }
}

const QString* DecoderEvent::errorMessage()
{
    return error_msg;
}

