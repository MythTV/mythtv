/*
	mfd_events.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods. These are explained in the header file.
*/

#include "mfd_events.h"


LoggingEvent::LoggingEvent(const QString & init_logging_string, int init_logging_verbosity)
             :QCustomEvent(65432)
{
    logging_string = init_logging_string;
    logging_verbosity = init_logging_verbosity;
}

const QString & LoggingEvent::getString()
{
    return logging_string;
}

int LoggingEvent::getVerbosity()
{
    return logging_verbosity;
}


/*
---------------------------------------------------------------------
*/

SocketEvent::SocketEvent(const QString &text_to_send, int identifier)
            :QCustomEvent(65431)
{
    text = text_to_send;
    socket_identifier = identifier;
}

const QString& SocketEvent::getString()
{
    return text;
}

int SocketEvent::getSocketIdentifier()
{
    return socket_identifier;
}

/*
---------------------------------------------------------------------
*/

AllClientEvent::AllClientEvent(const QString &text_to_send)
            :QCustomEvent(65430)
{
    text = text_to_send;
}

const QString& AllClientEvent::getString()
{
    return text;
}

/*
---------------------------------------------------------------------
*/

FatalEvent::FatalEvent(const QString &death_rattle, int plugin_identifier)
            :QCustomEvent(65429)
{
    text = death_rattle;
    plugin_int = plugin_identifier;
}

const QString& FatalEvent::getString()
{
    return text;
}

int FatalEvent::getPluginIdentifier()
{
    return plugin_int;
}

/*
---------------------------------------------------------------------
*/

ServiceEvent::ServiceEvent(const QString &service_string)
            :QCustomEvent(65428)
{
    text = service_string;
}

const QString& ServiceEvent::getString()
{
    return text;
}

/*
---------------------------------------------------------------------
*/

MetadataChangeEvent::MetadataChangeEvent(int which_collection)
            :QCustomEvent(65427)
{
    identifier = which_collection;
}

int MetadataChangeEvent::getIdentifier()
{
    return identifier;
}

/*
---------------------------------------------------------------------
*/

ShutdownEvent::ShutdownEvent()
            :QCustomEvent(65426)
{
}

