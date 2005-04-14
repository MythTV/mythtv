/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for a very simple plugin

*/

#include "dummy.h"
#include "../../mfd.h"

Dummy *a_dummy = NULL;

//
//  We define a very basic interface
//  into this plugin that the plugin
//  manager uses to get handles to
//  do basic things (init and stop)
//
//  Note that we don't just create a
//  function to get a pointer to the
//  plugin object. The MFD doesn't know
//  what kind of object the plugin is,
//  and it shouldn't care. 
//

extern "C" {
bool         mfdplugin_init(MFD*, int);
QStringList  mfdplugin_capabilities();
bool         mfdplugin_run();
void         mfdplugin_parse_tokens(const QStringList &tokens, int socket_identifier);
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
}


//
//  This is the interface that
//  brings the plugin into
//  existence
//

bool mfdplugin_init(MFD *owner, int identifier)
{
    a_dummy = new Dummy(owner, identifier);
    return true;
}

//
//  This returns a list of first
//  tokens that the object is
//  prepared to accept.
//

QStringList mfdplugin_capabilities()
{
    if(a_dummy)
    {
        return a_dummy->getCapabilities();
    }
    
    return NULL;
}

//
//  Start it executing (it's a QThread
//  object)
//

bool mfdplugin_run()
{
    if(a_dummy)
    {
        a_dummy->start();
        return true;
    }
    return false;
}

//
//  This is how the mfd passes commands
//  to this plugin. Note that the plugin
//  cannot use a QSocket directly to 
//  return data to the client, because
//  QSocket is not even reentrant, let
//  alone threadsafe. It uses a reference
//  (socket_identifier) in a CustomEvent 
//  that gets passed up to the mfd.
//

void mfdplugin_parse_tokens(const QStringList &tokens, int socket_identifier)
{
    if(a_dummy)
    {
        a_dummy->parseTokens(tokens, socket_identifier);
    }
}

//
//  This is how you kill it off
//  (before it gets unloaded by
//  the plugin manager)
//

bool mfdplugin_stop()
{
    if(a_dummy)
    {
        a_dummy->stop();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    if(a_dummy)
    {
        if(!a_dummy->running())
        {
            delete a_dummy;
            a_dummy = NULL;
            return true;
        }
        return false;
    }
    return true;
}
