#include <QString>
#include <QStringList>

#include "mythsocket.h"
#include "sockethandler.h"
#include "mythlogging.h"
#include "mythcorecontext.h"

#include "controlrequesthandler.h"

bool ControlRequestHandler::AnnounceSocket(void)
{
    if (!m_socket)
        return false;

    QString ann = QString("ANN MediaServer %1")
                        .arg(gCoreContext->GetHostName());
    QStringList strlist(ann);
    
    if (!m_socket->Announce(strlist))
        return false;
    return true;
}

bool ControlRequestHandler::HandleQuery(SocketHandler *socket,
                                    QStringList &commands, QStringList &slist)
{
    bool handled = false;
    QString command = commands[0];

/*
    if (command == "GO_TO_SLEEP")
        handled = HandleSleep();
    else if (command == "SHUTDOWN_NOW")
        handled = HandleShutdown();
*/

    return handled;
}
