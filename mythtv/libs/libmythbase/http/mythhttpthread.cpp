// MythTV
#include "mythlogging.h"
#include "http/mythhttpthreadpool.h"
#include "http/mythhttpserver.h"
#include "http/mythhttpsocket.h"
#include "http/mythhttpthread.h"

#define LOC (QString("%1: ").arg(objectName()))

MythHTTPThread::MythHTTPThread(MythHTTPServer* Server, MythHTTPConfig Config,
                               const QString& ThreadName, qintptr Socket, bool Ssl)
  : MThread(ThreadName),
    m_server(Server),
    m_config(std::move(Config)),
    m_socketFD(Socket),
    m_ssl(Ssl)
{
}

void MythHTTPThread::run()
{
    RunProlog();
    m_socket = new MythHTTPSocket(m_socketFD, m_ssl, m_config);
    QObject::connect(m_server, &MythHTTPServer::PathsChanged,    m_socket, &MythHTTPSocket::PathsChanged);
    QObject::connect(m_server, &MythHTTPServer::HandlersChanged, m_socket, &MythHTTPSocket::HandlersChanged);
    QObject::connect(m_server, &MythHTTPServer::ServicesChanged, m_socket, &MythHTTPSocket::ServicesChanged);
    QObject::connect(m_server, &MythHTTPServer::HostsChanged,    m_socket, &MythHTTPSocket::HostsChanged);
    QObject::connect(m_server, &MythHTTPServer::OriginsChanged,  m_socket, &MythHTTPSocket::OriginsChanged);
    QObject::connect(m_socket, &MythHTTPSocket::ThreadUpgraded,  m_server, &MythHTTPServer::ThreadUpgraded);
    exec();
    delete m_socket;
    m_socket = nullptr;
    RunEpilog();
}

/*! \brief Tell the socket to complete and disconnect.
 *
 * We use this mechanism as QThread::quit is a slot and we cannot use it to trigger
 * disconnection (and finished is too late for our needs). So tell the socket to
 * cleanup and close - and once the socket is disconnected it will trigger a call
 * to QThread::quit().
*/
void MythHTTPThread::Quit()
{
    if (m_socket)
        emit m_socket->Finish();
}
