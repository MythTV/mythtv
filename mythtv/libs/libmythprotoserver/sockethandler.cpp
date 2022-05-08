#include <QStringList>
#include <utility>

#include "libmythbase/mythlogging.h"
#include "sockethandler.h"

SocketHandler::SocketHandler(MythSocket *sock, MythSocketManager *parent,
                  QString hostname) :
        ReferenceCounter("SocketHandler"),
        m_socket(sock), m_parent(parent),
        m_hostname(std::move(hostname))
{
    if (m_socket)
        m_socket->IncrRef();
}

SocketHandler::~SocketHandler()
{
    if (m_socket)
    {
        m_socket->DecrRef();
        m_socket = nullptr;
    }
}

bool SocketHandler::WriteStringList(const QStringList &strlist)
{
    if (!m_socket)
        return false;

    return m_socket->WriteStringList(strlist);
}

bool SocketHandler::SendReceiveStringList(QStringList &strlist,
                                          uint min_reply_length)
{
    if (!m_socket)
        return false;

    return m_socket->SendReceiveStringList(strlist, min_reply_length);
}
