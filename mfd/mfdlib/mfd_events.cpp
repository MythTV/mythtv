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

ServiceEvent::ServiceEvent(
                            bool    l_please_broadcast,
                            bool    l_newly_created,
                            Service &a_service
                          )
            :QCustomEvent(65428)
{
    please_broadcast = l_please_broadcast;
    newly_created = l_newly_created;

    if(a_service.hostKnown())
    {
        service = new Service(
                                a_service.getName(),
                                a_service.getType(),
                                a_service.getHostname(),
                                a_service.getLocation(),
                                a_service.getPort()
                             );
    }
    else
    {
        service = new Service(
                                a_service.getName(),
                                a_service.getType(),
                                a_service.getLocation(),
                                a_service.getAddress(),
                                a_service.getPort()
                             );
    }                            
}

ServiceEvent::~ServiceEvent()
{
    if(service)
    {
        delete service;
        service = NULL;
    }
}

/*
---------------------------------------------------------------------
*/

MetadataChangeEvent::MetadataChangeEvent(int which_collection, bool external)
            :QCustomEvent(65427)
{
    identifier = which_collection;
    was_external = external;
}

int MetadataChangeEvent::getIdentifier()
{
    return identifier;
}

bool MetadataChangeEvent::getExternalFlag()
{
    return was_external;
}

/*
---------------------------------------------------------------------
*/

ShutdownEvent::ShutdownEvent()
            :QCustomEvent(65426)
{
}

