#ifndef MFD_EVENTS_H_
#define MFD_EVENTS_H_
/*
	mfd_events.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for custom events that the mfd and
	its various subobjects/threads can pass to
	each other.

*/

#include <qevent.h>
#include <qsocket.h>

#include "service.h"

class LoggingEvent : public QCustomEvent
{
    //
    //  Simple way to let any sub-object or thread
    //  post something to the log
    //

  public:

    LoggingEvent(const QString & init_logging_string, int init_logging_verbosity);
    const QString & getString();
    int getVerbosity();

  private:

    QString logging_string;
    int     logging_verbosity;
};

class SocketEvent : public QCustomEvent
{
    //
    //  A thread way down in the plugins
    //  somewhere can use this to send
    //  data down a socket inspite of the
    //  fact that QSocket's are not thread
    //  safe. The thread posts the event,
    //  and the main application sends the
    //  data (in the main application thread).
    //
    
  public:

    SocketEvent(const QString &text_to_send, int identifier);
    const QString & getString();
    int getSocketIdentifier();
    
  private:
  
    QString text;
    int socket_identifier;
};

class AllClientEvent: public QCustomEvent
{
    //
    //  Like the above SocketEvent, this lets anything anywhere (at any
    //  "thread depth" send a message, but it goes to *all* currently
    //  connected clients)
    //

  public:
  
    AllClientEvent(const QString &text_to_send);
    const QString & getString();
    
  private:
  
    QString text;
    
};

class FatalEvent : public QCustomEvent
{
    //
    //  A way for a plugin to say that something bad
    //  has happened and to ask to be unloaded
    //

  public:
  
    FatalEvent(const QString &death_rattle, int plugin_identifier);
    const QString & getString();
    int getPluginIdentifier();
    
  private:
  
    QString text;
    int     plugin_int;
    
};

class ServiceEvent : public QCustomEvent
{
    //
    //  ServiceEvents are used for two purposes:
    //
    //  1. If a plugin wants a service it offers to be "broadcast" (eg. http
    //  plugin wants to say it's ready to offer html) then it sends one of
    //  these with the please_broadcast flag set to true and the
    //  newly_created flag set to true. When it's done (no longer wants to
    //  offer html), it changes the newly_created flag to false.
    //
    //  2. If a service *discovering* plugin (currently only zc_client, but
    //  in the future, who knows ...) finds a service, it sends one with
    //  please_broadcast set to false (the service is already being
    //  broadcast), but with newly_created set to true. If the service then
    //  goes away, it sends one with both please_broadcast and newly_created
    //  set to false.
    //
  
  public:
    
    ServiceEvent(
                    bool    l_please_broadcast,
                    bool    l_newly_created,
                    Service &a_service
                );

    bool        pleaseBroadcast(){ return please_broadcast; }
    bool        newlyCreated(){ return newly_created; }
    Service*    getService(){ return service;}

    ~ServiceEvent();
                
  private:

    bool    please_broadcast;
    bool    newly_created;
    Service *service;
    
};

class MetadataChangeEvent : public QCustomEvent
{
    //
    //  Whenever any metadata container/monitor has found new/changed
    //  metadata, it emits this. If something else changed it, that
    //  something else should set the external flag true.
    //
    
  public:
  
    MetadataChangeEvent(int which_collection, bool external = false);
    int getIdentifier();
    bool getExternalFlag();
    
  private:
  
    int identifier;
    bool was_external;
};

class ShutdownEvent : public QCustomEvent
{
    //
    //  When anything wants us to shutdown. This is used by the signal
    //  thread to asynchronously tell the mfd to shutdown on SIG INT/TERM
    //
    
  public:
  
    ShutdownEvent();
};

#endif
