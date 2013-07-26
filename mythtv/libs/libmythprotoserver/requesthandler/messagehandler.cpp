#include "mythcorecontext.h"
#include "mythevent.h"

#include "requesthandler/messagehandler.h"

MessageHandler::MessageHandler(void) : SocketRequestHandler()
{
    if (!gCoreContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "MessageHandler started with no CoreContext!");
        return;
    }
    gCoreContext->addListener(this);
}

void MessageHandler::customEvent(QEvent *e)
{
    if ((MythEvent::Type)(e->type()) != MythEvent::MythEventMessage)
        return;

    if (!gCoreContext->IsMasterBackend())
        // only master backend should forward events
        return;

    //MythEvent *me = (MythEvent *)e;

    // TODO: actually do something
    // right now, this really doesnt need to do anything, but at such time as
    // the backend's mainserver.cpp is migrated over, this will need to
    // its message passing behavior
}

bool MessageHandler::HandleQuery(SocketHandler *socket, QStringList &commands,
                                 QStringList &slist)
{
    QString command = commands[0];
    bool res = false;

    if (command == "MESSAGE")
        res = HandleInbound(socket, slist);
    else if (command == "BACKEND_MESSAGE")
        res = HandleOutbound(socket, slist);

    return res;
}

// inbound events from clients
bool MessageHandler::HandleInbound(SocketHandler *sock, QStringList &slist)
{
    QStringList res;
    if (slist.size() < 2)
    {
        res << "ERROR" << "Insufficient Length";
        sock->WriteStringList(res);
        return true;
    }

    QString message = slist[1];
    QStringList extra_data;
    for (uint i = 2; i < (uint) slist.size(); i++)
        extra_data.push_back(slist[i]);

    if (extra_data.empty())
    {
        MythEvent me(message);
        gCoreContext->dispatch(me);
    }
    else
    {
        MythEvent me(message, extra_data);
        gCoreContext->dispatch(me);
    }

    res << "OK";
    sock->WriteStringList(res);
    return true;
}

// outbound events from master backend
bool MessageHandler::HandleOutbound(SocketHandler *sock, QStringList &slist)
{
    QStringList::const_iterator iter = slist.begin();
    QString message = *(iter++);
    QStringList extra_data( *(iter++) );
    ++iter;
    for (; iter != slist.end(); ++iter)
        extra_data << *iter;
    MythEvent me(message, extra_data);
    gCoreContext->dispatch(me);
    return true;
}

