/*
	threadevents.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for thread <--> mtd communication
	events.


*/

#include "threadevents.h"

LoggingEvent::LoggingEvent(const QString & init_logging_string)
            :QCustomEvent(65432)
{
    logging_string = init_logging_string;
}

const QString & LoggingEvent::getString()
{
    return logging_string;
}


ErrorEvent::ErrorEvent(const QString & init_error_string)
            :QCustomEvent(65431)
{
    error_string = init_error_string;
}

const QString & ErrorEvent::getString()
{
    return error_string;
}

