#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythevent.h"

#include "requesthandler/messagehandler.h"

MessageHandler::MessageHandler(void)
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
    if (e->type() != MythEvent::kMythEventMessage)
        return;

    if (!gCoreContext->IsMasterBackend())
        // only master backend should forward events
        return;

    //MythEvent *me = static_cast<MythEvent *>(e);

    // TODO: actually do something
    // right now, this really doesnt need to do anything, but at such time as
    // the backend's mainserver.cpp is migrated over, this will need to
    // its message passing behavior
}

bool MessageHandler::HandleQuery(SocketHandler *socket, QStringList &commands,
                                 QStringList &slist)
{
    const QString& command = commands[0];
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

    const QString& message = slist[1];
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

/**
 * \fn MessageHandler::HandleOutbound
 *
 * Handle an asynchronous message received from the master backend.
 * These are converted into a MythEvent and dispatched to all classes
 * registered for events.
 *
 * \param sock  The socket on which the message arrived. This class of
 *              messages are never responded to, so this variable is
 *              never used.
 * \param slist The contents of the asynchronous message. This is put
 *              into an event and sent to any classes that are
 *              interested.
 */
bool MessageHandler::HandleOutbound(SocketHandler */*sock*/, QStringList &slist)
{
    QStringList::const_iterator iter = slist.cbegin();
    QString message = *(iter++);
    QStringList extra_data( *(iter++) );
    ++iter;
    for (; iter != slist.cend(); ++iter)
        extra_data << *iter;
    MythEvent me(message, extra_data);
    gCoreContext->dispatch(me);
    return true;
}

