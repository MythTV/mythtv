/*
	dummy.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for a very simple plugin.

*/

#include "dummy.h"

Dummy::Dummy(MFD *owner, int identity)
      :MFDCapabilityPlugin(owner, identity)
{

    //
    //  The MFD's constructor **needs** to be
    //  called, and the plugin should list
    //  command tokens it can handle
    //
    
    my_capabilities.append("dummy");

    //
    //  Send a highest verbosity log message
    //

    log("a dummy plugin is being created", 10);

    //
    //  Chutt?
    //
    //  Uncomment the first, all is ok.
    //  Uncomment the second, all hell breaks out
    //
   
    // owner->shallowPrintHello();
    // owner->deepPrintHello();
   
    

}

void Dummy::doSomething(const QStringList &tokens, int socket_identifier)
{
    //
    //  doSomething() is actually being called from within a separate
    //  thread, so you can do whatever processing you like (within
    //  reason ... you shouldn't just block indefinitely).
    //
    
    if(tokens.count() > 0)
    {
        if(tokens[0] == "dummy")
        {
            if(tokens.count() > 1)
            {
                if(tokens[1] == "die")
                {
                    //
                    //  Simulate what a plugin should do when it
                    //  encounters a situation where it should be
                    //  killed off and unloaded
                    //
                    
                    fatal("dummy plugin was asked to die");
                }
                else
                {
                    huh(tokens, socket_identifier);
                }
            }
            else
            {
                sendMessage("dummy exists", socket_identifier);
            }
            
            //
            //  If you want to simulate a load to see if all 
            //  requests are getting through, etc., then
            //  uncomment this
            //
            //  sleep(5);
        }
        else
        {
            huh(tokens, socket_identifier);
        }
    }
    else
    {
        huh(tokens, socket_identifier);
    }
}

Dummy::~Dummy()
{
    //
    //  Announce our demise, in a verbose kind of way.
    //

    log("a dummy plugin is being destroyed", 10);
}

