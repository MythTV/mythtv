/*
    threadevents.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Methods for thread <--> mtd communication
    events.


*/

#include "threadevents.h"

QEvent::Type LoggingEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ErrorEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

LoggingEvent::LoggingEvent(const QString &init_logging_string) :
    QEvent(kEventType), logging_string(init_logging_string)
{
}

const QString &LoggingEvent::getString()
{
    return logging_string;
}

ErrorEvent::ErrorEvent(const QString &init_error_string) :
    QEvent(kEventType), error_string(init_error_string)
{
}

const QString &ErrorEvent::getString()
{
    return error_string;
}
