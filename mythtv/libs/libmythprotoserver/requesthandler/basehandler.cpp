#include <QString>
#include <QStringList>

#include "mythsystemevent.h"
#include "mythsocket.h"
#include "mythsocketmanager.h"
#include "socketrequesthandler.h"
#include "sockethandler.h"
#include "mythmiscutil.h"
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythtimezone.h"

#include "requesthandler/basehandler.h"

bool BaseRequestHandler::HandleAnnounce(MythSocket *socket,
                                QStringList &commands, QStringList &slist)
{
    if (commands.size() != 4)
        return false;

    bool blockShutdown;
    if (commands[1] == "Playback")
        blockShutdown = true;
    else if (commands[1] == "Monitor")
        blockShutdown = false;
    else
        return false;

    QString hostname = commands[2];

    int eventlevel = commands[3].toInt();
    bool systemevents = ( (eventlevel == 1) || (eventlevel == 3));
    bool normalevents = ( (eventlevel == 1) || (eventlevel == 2));

    SocketHandler *handler = new SocketHandler(socket, m_parent, hostname);
    socket->SetAnnounce(slist);

    handler->BlockShutdown(blockShutdown);
    handler->AllowStandardEvents(normalevents);
    handler->AllowSystemEvents(systemevents);

    m_parent->AddSocketHandler(handler);

    handler->WriteStringList(QStringList("OK"));
    handler->DecrRef();
    handler = NULL;

    LOG(VB_GENERAL, LOG_DEBUG, QString("MainServer::ANN %1")
                                    .arg(commands[1]));
    LOG(VB_GENERAL, LOG_NOTICE, QString("adding: %1 as a client (events: %2)")
                               .arg(commands[2]).arg(eventlevel));
    gCoreContext->SendSystemEvent(QString("CLIENT_CONNECTED HOSTNAME %1")
                                  .arg(commands[2]));

    return true;
}

bool BaseRequestHandler::HandleQuery(SocketHandler *sock,
                                QStringList &commands, QStringList &slist)
{
    QString command = commands[0];
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

    double loads[3];
    if (getloadavg(loads,3) == -1)
    {
        strlist << "ERROR";
        strlist << "getloadavg() failed";
    }
    else
        strlist << QString::number(loads[0])
                << QString::number(loads[1])
                << QString::number(loads[2]);

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
    time_t      uptime;

    if (getUptime(uptime))
        strlist << QString::number(uptime);
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
    int         totalMB, freeMB, totalVM, freeVM;

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

