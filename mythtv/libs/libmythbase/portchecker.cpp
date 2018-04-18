//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 MythTV Developers <mythtv-dev@mythtv.org>
//
// This is part of MythTV (https://www.mythtv.org)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpSocket>
#include <QEventLoop>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>

#include <thread>

#include "mythcorecontext.h"
#include "mythtimer.h"
#include "portchecker.h"

#define LOC QString("PortChecker::%1(): ").arg(__func__)

PortChecker::PortChecker() :
    cancelCheck(false)
{
}

PortChecker::~PortChecker()
{
}

/**
 * Check if a port is open and sort out the link-local scope.
 *
 * Checks the specified port repeatedly
 * until either it connects or the time limit is reached
 *
 * This routine also finds the correct scope id in case of an
 * IPV6 link-local address, and caches it in
 * gCoreContext->SetScopeForAddress
 *
 * If linkLocalOnly is specified, it only obtains link-local
 * address scope.
 * In this case, the port is not checked unless needed to
 * find the scope. This will also return after 1 check of all available
 * interfaces. If will not repeatedly check the port. To make sure all
 * interfaces are checked, make sure enough time is allowed, up to 3
 * seconds for each interface checked.
 *
 * For Windows build, link local address is not checked as windows
 * does not require the scope id.
 *
 * This routine does call event processor, so the GUI can be responsive
 * on the same thread.
 *
 * \param host [in,out] Host id or ip address (IPV4 or IPV6).
 * This is updated with scope if the address is link-local IPV6.
 * \param port Port number to check.
 * \param timeLimit limit in milliseconds for testing.
 * \param linkLocalOnly Only obtain Link-local address scope.
 * \return true if the port could be contacted. In the case
 * of linkLocalOnly, return true if it was link local and
 * was changed, false in other cases.
*/
bool PortChecker::checkPort(QString &host, int port, int timeLimit, bool linkLocalOnly)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("host %1 port %2 timeLimit %3 linkLocalOnly %4")
        .arg(host).arg(port).arg(timeLimit).arg(linkLocalOnly));
    cancelCheck = false;
    QHostAddress addr;
    bool isIPAddress = addr.setAddress(host);
    bool islinkLocal = false;
// Windows does not need the scope on the ip address so we can skip
// some processing
#ifndef _WIN32
    if (isIPAddress
      && addr.protocol() == QAbstractSocket::IPv6Protocol
      && addr.isInSubnet(QHostAddress::parseSubnet("fe80::/10")))
        islinkLocal = true;
#endif
    if (linkLocalOnly)
    {
        if (islinkLocal)
        {
            // If we already know the scope, set it here and return
            if (gCoreContext->GetScopeForAddress(addr))
            {
                host = addr.toString();
                return true;
            }
        }
        else
            return false;
    }
    QList<QNetworkInterface> cards = QNetworkInterface::allInterfaces();
    QListIterator<QNetworkInterface> iCard = cards;
    MythTimer timer(MythTimer::kStartRunning);
    QTcpSocket socket(this);
    QAbstractSocket::SocketState state = QAbstractSocket::UnconnectedState;
    int retryCount = 0;
    QString scope;
    bool testedAll = false;
    while (state != QAbstractSocket::ConnectedState
        && (timer.elapsed() < timeLimit))
    {
        if (state == QAbstractSocket::UnconnectedState)
        {
// Windows does not need the scope on the ip address so we can skip
// some processing
#ifndef _WIN32
            int iCardsEnd = 0;
            if (islinkLocal && !gCoreContext->GetScopeForAddress(addr))
            {
                addr.setScopeId(QString());
                while (addr.scopeId().isEmpty() && iCardsEnd<2)
                {
                    // search for the next available IPV6 interface.
                    if (iCard.hasNext())
                    {
                        QNetworkInterface card = iCard.next();
                        LOG(VB_GENERAL, LOG_DEBUG, QString("Trying interface %1").arg(card.name()));
                        unsigned int flags = card.flags();
                        if ((flags & QNetworkInterface::IsLoopBack)
                         || !(flags & QNetworkInterface::IsRunning))
                            continue;
                        // check that IPv6 is enabled on that interface
                        QList<QNetworkAddressEntry> addresses = card.addressEntries();
                        bool foundv6 = false;
                        foreach (QNetworkAddressEntry ae, addresses)
                        {
                            if (ae.ip().protocol() == QAbstractSocket::IPv6Protocol)
                            {
                                foundv6 = true;
                                break;
                            }
                        }
                        if (foundv6)
                        {
                            scope = card.name();
                            addr.setScopeId(scope);
                            break;
                        }
                    }
                    else
                    {
                        // Get a new list in case a new interface
                        // has been added.
                        cards = QNetworkInterface::allInterfaces();
                        iCard = cards;
                        iCard.toFront();
                        testedAll=true;
                        iCardsEnd++;
                    }
                }
            }
            if (iCardsEnd > 1)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("There is no IPV6 compatible interface for %1")
                  .arg(host));
                break;
            }
#endif
            QString dest;
            if (isIPAddress)
                dest=addr.toString();
            else
                dest=host;
            socket.connectToHost(dest, port);
            retryCount=0;
        }
        else
            retryCount++;
        // This retry count of 6 means 3 seconds of waiting for
        // connection before aborting and starting a new connection attempt.
        if (retryCount > 6)
            socket.abort();
        processEvents();
        // Check if user got impatient and canceled
        if (cancelCheck)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        state = socket.state();
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("socket state %1")
            .arg(state));
        if (linkLocalOnly
          && state == QAbstractSocket::UnconnectedState
          && testedAll)
            break;
    }
    if (state == QAbstractSocket::ConnectedState
      && islinkLocal && !scope.isEmpty())
    {
       gCoreContext->SetScopeForAddress(addr);
       host = addr.toString();
    }
    socket.abort();
    processEvents();
    return (state == QAbstractSocket::ConnectedState);
}

/**
 * Convenience method to resolve link-local address.
 *
 * Update a host id to include the correct scope if it
 * is link-local. If this is called with anything that
 * is not a link-local address, it remains unchanged.
 *
 * \param host [in,out] Host id or ip address (IPV4 or IPV6).
 * This is updated with scope if the address is link-local IPV6.
 * \param port Port number to check.
 * \param timeLimit limit in milliseconds for testing.
 * \return true if it was link local and was resolved,
 * false in other cases.
*/
// static method
bool PortChecker::resolveLinkLocal(QString &host, int port, int timeLimit)
{
    PortChecker checker;
    return checker.checkPort(host,port,timeLimit,true);
}

void PortChecker::processEvents(void)
{
    qApp->processEvents(QEventLoop::AllEvents, 250);
    qApp->processEvents(QEventLoop::AllEvents, 250);
}

/**
 * Cancel the checkPort operation currently in progress.
 *
 * This is a slot that can be used by a GUI to stop port
 * check operation in progress in case the user wants
 * to cancel it.
*/
void PortChecker::cancelPortCheck(void)
{
    cancelCheck = true;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
