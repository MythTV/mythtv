/*
	zc_supervisor.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    !! This code is covered the the Apple Public Source License !!
    
    See ./apple/APPLE_LICENSE

*/

#include <qapplication.h>

#include "zc_supervisor.h"

ZeroConfigSupervisor::ZeroConfigSupervisor(MFD *owner, int identity, 
                                           ZeroConfigClient *a_client,
                                           ZeroConfigResponder *a_responder)
      :MFDCapabilityPlugin(owner, identity)
{
    zero_config_client = a_client;
    zero_config_responder = a_responder;
    log("a zeroconfig plugin is being created", 10);
    my_capabilities.append("services");
}

void ZeroConfigSupervisor::runOnce()
{
    zero_config_client->start();
    zero_config_responder->start();
}

void ZeroConfigSupervisor::doSomething(const QStringList &tokens, int socket_identifier)
{
    //
    //  We get mfdp "services" requests here, and just pass them
    //  to either the client (e.g. services list ...) or the
    //  responder (e.g. services add ...)
    //

    if(tokens.count() < 2)
    {
        warning(QString("zeroconfig supervisor was passed a request without sufficient tokens: %1")
                .arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
        return;
    }

    if(tokens[0] != "services")
    {
        warning(QString("zeroconfig supervisor was passed a set of tokens that did not begin with services: %1")
                .arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
        return;
    }

    if(tokens[1] == "list")
    {
        zero_config_client->addRequest(tokens, socket_identifier);
        zero_config_client->wakeUp();
        return;
    }
    else if(tokens[1] == "add" ||
            tokens[1] == "remove")
    {
        zero_config_responder->addRequest(tokens, socket_identifier);
        zero_config_responder->wakeUp();
        return;
    }
    
    warning(QString("zeroconfig supervisor was passed a request it doesn't understand: %1")
            .arg(tokens.join(" ")));
    huh(tokens, socket_identifier);
}



ZeroConfigSupervisor::~ZeroConfigSupervisor()
{
    log("zeroconfig supervisor is being killed off", 10);
}
