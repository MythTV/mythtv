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
    //  This is used to pass a message up from a plugin and then back down
    //  to services handling plugin. 'fer example, the audio plugin needs to
    //  let the zeroconfig responder plugin know about where and how to
    //  connect to the macp (Myth Audio Control Protocol). This is how it
    //  does so
    //
  
  public:
    
    ServiceEvent(const QString &service_string);
    const QString & getString();
    
  private:

    QString text;
    
};

class MetadataChangeEvent : public QCustomEvent
{
    //
    //  Whenever any metadata container/monitor has found new/changed
    //  metadata, it emits this.
    //
    
  public:
  
    MetadataChangeEvent(int which_collection);
    int getIdentifier();
    
  private:
  
    int identifier;
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
