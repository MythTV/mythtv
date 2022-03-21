// Qt
#include <QString>
#include <QStringList>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsocket.h"
#include "libmythprotoserver/sockethandler.h"

// MythMediaServer
#include "controlrequesthandler.h"

bool ControlRequestHandler::AnnounceSocket(void)
{
    if (!m_socket)
        return false;

    QString ann = QString("ANN MediaServer %1")
                        .arg(gCoreContext->GetHostName());
    QStringList strlist(ann);
    
    return m_socket->Announce(strlist);
}

/**
 * \fn ControlRequestHandler::HandleQuery
 * \brief Dispatch query messages received from a client.
 *
 * \param socket   The socket on which the message arrived. This is
 *                 used to communicate the response back to the sender
 *                 of the query.
 * \param commands The command to execute.
 * \param slist    Additional list arguments to the query. None of the
 *                 supported queries have any additional arguments, so
 *                 this variable is never used.
 */
bool ControlRequestHandler::HandleQuery(SocketHandler */*socket*/,
                                        QStringList &/*commands*/,
                                        QStringList &/*slist*/)
{
    bool handled = false;
#if 0
    QString command = commands[0];

    if (command == "GO_TO_SLEEP")
        handled = HandleSleep();
    else if (command == "SHUTDOWN_NOW")
        handled = HandleShutdown();
#endif

    return handled;
}
