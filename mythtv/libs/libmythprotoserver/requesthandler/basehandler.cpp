#include <QString>
#include <QStringList>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythsocket.h"
#include "libmythbase/mythtimezone.h"
#include "libmythtv/mythsystemevent.h"

#include "mythsocketmanager.h"
#include "sockethandler.h"
#include "socketrequesthandler.h"

#include "requesthandler/basehandler.h"

bool BaseRequestHandler::HandleAnnounce(MythSocket *socket,
                                QStringList &commands, QStringList &slist)
{
    if (commands.size() != 4)
        return false;

    bool blockShutdown = true;
    if (commands[1] == "Playback")
        blockShutdown = true;
    else if (commands[1] == "Monitor")
        blockShutdown = false;
    else
        return false;

    const QString& hostname = commands[2];

    int eventlevel = commands[3].toInt();
    bool systemevents = ( (eventlevel == 1) || (eventlevel == 3));
    bool normalevents = ( (eventlevel == 1) || (eventlevel == 2));

    auto *handler = new SocketHandler(socket, m_parent, hostname);
    socket->SetAnnounce(slist);

    handler->BlockShutdown(blockShutdown);
    handler->AllowStandardEvents(normalevents);
    handler->AllowSystemEvents(systemevents);

    m_parent->AddSocketHandler(handler);

    handler->WriteStringList(QStringList("OK"));
    handler->DecrRef();
    handler = nullptr;

    LOG(VB_GENERAL, LOG_DEBUG, QString("MainServer::ANN %1")
                                    .arg(commands[1]));
    LOG(VB_GENERAL, LOG_NOTICE, QString("adding: %1 as a client (events: %2)")
                               .arg(commands[2]).arg(eventlevel));
    gCoreContext->SendSystemEvent(QString("CLIENT_CONNECTED HOSTNAME %1")
                                  .arg(commands[2]));

    return true;
}

/**
 * \fn BaseRequestHandler::HandleQuery
 * \brief Dispatch query messages received from a client.
 *
 * \param sock     The socket on which the message arrived. This is
 *                 used to communicate the response back to the sender
 *                 of the query.
 * \param commands The command to execute.
 * \param slist    Additional list arguments to the query. None of the
 *                 supported queries have any additional arguments, so
 *                 this variable is never used.
 */
bool BaseRequestHandler::HandleQuery(SocketHandler *sock,
                                     QStringList &commands,
                                     QStringList &/*slist*/)
{
    const QString& command = commands[0];
    bool res = false;

    if (command == "QUERY_LOAD")
        res = HandleQueryLoad(sock);
    else if (command == "QUERY_UPTIME")
        res = HandleQueryUptime(sock);
    else if (command == "QUERY_HOSTNAME")
        res = HandleQueryHostname(sock);
    else if (command == "QUERY_MEMSTATS")
        res = HandleQueryMemStats(sock);
    else if (command == "QUERY_TIME_ZONE")
        res = HandleQueryTimeZone(sock);

    return res;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_LOAD
 * Returns the Unix load on this backend
 * (three floats - the average over 1, 5 and 15 mins).
 */
bool BaseRequestHandler::HandleQueryLoad(SocketHandler *sock)
{
    QStringList strlist;

#if defined(_WIN32) || defined(Q_OS_ANDROID)
    strlist << "ERROR";
    strlist << "getloadavg() not supported";
#else
    loadArray loads = getLoadAvgs();
    if (loads[0] == -1)
    {
        strlist << "ERROR";
        strlist << "getloadavg() failed";
    }
    else
    {
        strlist << QString::number(loads[0])
                << QString::number(loads[1])
                << QString::number(loads[2]);
    }
#endif

    sock->WriteStringList(strlist);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_UPTIME
 * Returns the number of seconds this backend's host has been running
 */
bool BaseRequestHandler::HandleQueryUptime(SocketHandler *sock)
{
    QStringList strlist;
    std::chrono::seconds uptime = 0s;

    if (getUptime(uptime))
        strlist << QString::number(uptime.count());
    else
    {
        strlist << "ERROR";
        strlist << "Could not determine uptime.";
    }

    sock->WriteStringList(strlist);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_HOSTNAME
 * Returns the hostname of this backend
 */
bool BaseRequestHandler::HandleQueryHostname(SocketHandler *sock)
{
    QStringList strlist;

    strlist << gCoreContext->GetHostName();

    sock->WriteStringList(strlist);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_MEMSTATS
 * Returns total RAM, free RAM, total VM and free VM (all in MB)
 */
bool BaseRequestHandler::HandleQueryMemStats(SocketHandler *sock)
{
    QStringList strlist;
    int totalMB = 0;
    int freeMB  = 0;
    int totalVM = 0;
    int freeVM  = 0;

    if (getMemStats(totalMB, freeMB, totalVM, freeVM))
    {
        strlist << QString::number(totalMB) << QString::number(freeMB)
                << QString::number(totalVM) << QString::number(freeVM);
    }
    else
    {
        strlist << "ERROR"
                << "Could not determine memory stats.";
    }

    sock->WriteStringList(strlist);
    return true;
}

/**
 * \addtogroup myth_network_protocol
 * \par        QUERY_TIME_ZONE
 * Returns time zone ID, current offset, current time
 */
bool BaseRequestHandler::HandleQueryTimeZone(SocketHandler *sock)
{
    QStringList strlist;
    strlist << MythTZ::getTimeZoneID()
            << QString::number(MythTZ::calc_utc_offset())
            << MythDate::current_iso_string(true);

    sock->WriteStringList(strlist);
    return true;
}

