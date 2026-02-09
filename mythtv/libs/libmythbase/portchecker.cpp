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

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QtSystemDetection>
#endif
#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpSocket>
#include <QEventLoop>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>

#include <thread>

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythtimer.h"
#include "portchecker.h"

#define LOC QString("PortChecker::%1(): ").arg(__func__)

/**
 * Check if a port is open.
 *
 * Checks the specified port repeatedly
 * until either it connects or the time limit is reached
 *
 * This routine also finds the correct scope id in case of an
 * IPV6 link-local address, and caches it in
 * gCoreContext->SetScopeForAddress
 *
 * This routine does call event processor, so the GUI can be responsive
 * on the same thread.
 *
 * \param host Host id or ip address (IPV4 or IPV6).
 * \param port Port number to check.
 * \param timeLimit limit in milliseconds for testing.
 * \return true if the port could be contacted.
*/
bool PortChecker::checkPort(const QString &host, int port, std::chrono::milliseconds timeLimit)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("host %1 port %2 timeLimit %3")
        .arg(host).arg(port).arg(timeLimit.count()));
    m_cancelCheck = false;
// Windows does not need the scope on the ip address so we can skip
// some processing
#ifndef Q_OS_WINDOWS
    QHostAddress addr;
    bool isIPAddress = addr.setAddress(host);
    if (isIPAddress
      && addr.protocol() == QAbstractSocket::IPv6Protocol
      && addr.isInSubnet(QHostAddress::parseSubnet("fe80::/10")))
    {
        QString dest {host};
        return resolveLinkLocal(dest, port, timeLimit);
    }
#endif
    QTcpSocket socket;
    socket.connectToHost(host, port);
    MythTimer timer(MythTimer::kStartRunning);
    QAbstractSocket::SocketState state = QAbstractSocket::UnconnectedState;
    while (state != QAbstractSocket::ConnectedState
           && (timer.elapsed() < timeLimit)
           && !m_cancelCheck
           )
    {
        static constexpr std::chrono::milliseconds k_poll_interval {1ms};
        QCoreApplication::processEvents(QEventLoop::AllEvents, k_poll_interval.count());
        std::this_thread::sleep_for(1ns); // force thread to yield
        state = socket.state();
    }
    state = socket.state();
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("host %1 port %2 socket state %3, attempt time: %4")
        .arg(host, QString::number(port), QString::number(state),
                QString::number(timer.elapsed().count())
                )
        );
    return (state == QAbstractSocket::ConnectedState);
}

/**
 * Convenience method to resolve link-local address.
 *
 * Update a host id to include the correct scope if it
 * is link-local. If this is called with anything that
 * is not a link-local address, it remains unchanged.
 *
 * Check if a port is open and sort out the link-local scope.
 *
 * Checks the specified port repeatedly
 * until either it connects or the time limit is reached
 *
 * This routine also finds the correct scope id in case of an
 * IPV6 link-local address, and caches it in
 * gCoreContext->SetScopeForAddress
 *
 * The port is not checked unless needed to
 * find the scope. This will also return after 1 check of all available
 * interfaces. It will not repeatedly check the port. To make sure all
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
 * \return true if it was link local and was resolved,
 * false in other cases.
*/
bool PortChecker::resolveLinkLocal(QString &host, int port, std::chrono::milliseconds timeLimit)
{
    // Windows does not need the scope on the ip address so we can skip
    // some processing
#ifdef Q_OS_WINDOWS
    return false;
#else
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("host %1 port %2 timeLimit %3")
        .arg(host).arg(port).arg(timeLimit.count()));
    m_cancelCheck = false;
    QHostAddress addr;
    bool isIPAddress = addr.setAddress(host);
    if (isIPAddress
      && addr.protocol() == QAbstractSocket::IPv6Protocol
      && addr.isInSubnet(QHostAddress::parseSubnet("fe80::/10")))
    {
        // If we already know the scope, set it here and return
        if (gCoreContext->GetScopeForAddress(addr))
        {
            host = addr.toString();
            return true;
        }
    }
    else
    {
        return false;
    }
    QList<QNetworkInterface> cards = QNetworkInterface::allInterfaces();
    auto iCard = cards.cbegin();
    MythTimer timer(MythTimer::kStartRunning);
    QAbstractSocket::SocketState state = QAbstractSocket::UnconnectedState;
    int iCardsEnd = 0;
    while (state != QAbstractSocket::ConnectedState
           && (timer.elapsed() < timeLimit)
           && !m_cancelCheck
           )
    {
                addr.setScopeId(QString());
                while (addr.scopeId().isEmpty())
                {
                    // search for the next available IPV6 interface.
                    if (iCard != cards.cend())
                    {
                        LOG(VB_GENERAL, LOG_DEBUG, QString("Trying interface %1").arg(iCard->name()));
                        unsigned int flags = iCard->flags();
                        if (!(flags & QNetworkInterface::IsLoopBack)
                            && (flags & QNetworkInterface::IsRunning))
                        {
                        // check that IPv6 is enabled on that interface
                        QList<QNetworkAddressEntry> addresses = iCard->addressEntries();
                        for (const auto& ae : std::as_const(addresses))
                        {
                            if (ae.ip().protocol() == QAbstractSocket::IPv6Protocol)
                            {
                                addr.setScopeId(iCard->name());
                                break;
                            }
                        }
                        }
                        iCard++;
                    }
                    else
                    {
                        iCardsEnd++;
                        if (iCardsEnd >= 2)
                        {
                            LOG(VB_GENERAL, LOG_ERR, LOC +
                                QString("There is no IPV6 compatible interface for %1").arg(host)
                                );
                            return false;
                        }
                        // Get a new list in case a new interface
                        // has been added.
                        cards = QNetworkInterface::allInterfaces();
                        iCard = cards.cbegin();
                    }
                }
        QTcpSocket socket;
        socket.connectToHost(addr.toString(), port);

        MythTimer attempt_time {MythTimer::kStartRunning};
        static constexpr std::chrono::milliseconds k_attempt_time_limit {3s};
        static constexpr std::chrono::milliseconds k_poll_interval {1ms};
        static constexpr std::chrono::milliseconds k_log_interval {100ms};
        std::chrono::milliseconds next_log {k_log_interval};
        while (state != QAbstractSocket::ConnectedState
               && !m_cancelCheck
               && (timer.elapsed() < timeLimit)
               && attempt_time.elapsed() < k_attempt_time_limit
               )
        {
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents, k_poll_interval.count());
                std::this_thread::sleep_for(1ns); // force thread to be de-scheduled
            }
            state = socket.state();
            if (attempt_time.elapsed() > next_log)
            {
                next_log += k_log_interval;
#if 0
                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("host %1 port %2 socket state %3, attempt time: %4")
                    .arg(host, QString::number(port), QString::number(state),
                         QString::number(attempt_time.elapsed().count())
                         )
                    );
#endif
            }
        }
        state = socket.state();
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("host %1 port %2 socket state %3, attempt time: %4")
            .arg(host, QString::number(port), QString::number(state),
                 QString::number(attempt_time.elapsed().count())
                 )
            );
    }
    if (state == QAbstractSocket::ConnectedState && !addr.scopeId().isEmpty())
    {
       gCoreContext->SetScopeForAddress(addr);
       host = addr.toString();
    }
    return (state == QAbstractSocket::ConnectedState);
#endif
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
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Aborting port check"));
    m_cancelCheck = true;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
