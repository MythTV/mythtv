
using namespace std;

#include <QStringList>

#include "sockethandler.h"
#include "mythlogging.h"

SocketHandler::SocketHandler(MythSocket *sock, MythSocketManager *parent,
                  QString hostname) :
        ReferenceCounter("SocketHandler"),
        m_blockShutdown(false), m_standardEvents(false),
        m_systemEvents(false), m_socket(sock), m_parent(parent),
        m_hostname(hostname)
{
    if (m_socket)
        m_socket->IncrRef();
}

SocketHandler::~SocketHandler()
{
    if (m_socket)
    {
        m_socket->DecrRef();
        m_socket = NULL;
    }
}

bool SocketHandler::SendStringList(QStringList &strlist, bool lock)
{
    if (!m_socket)
        return false;

    LOG(VB_GENERAL, LOG_DEBUG, "Locking Socket for write");
    if (lock) m_socket->Lock();
    bool res = m_socket->writeStringList(strlist);
    if (lock) m_socket->Unlock();
    LOG(VB_GENERAL, LOG_DEBUG, "UnLocking Socket from write");

    return res;
}

bool SocketHandler::SendReceiveStringList(QStringList &strlist,
                                          uint min_reply_length)
{
    if (!m_socket)
        return false;

    return m_socket->SendReceiveStringList(strlist, min_reply_length);
}
