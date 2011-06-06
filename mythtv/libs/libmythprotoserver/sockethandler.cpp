
using namespace std;

#include <QStringList>

#include "sockethandler.h"
#include "mythverbose.h"

SocketHandler::SocketHandler(MythSocket *sock, MythSocketManager *parent,
                  QString hostname) :
        ReferenceCounter(), m_blockShutdown(false), m_standardEvents(false),
        m_systemEvents(false), m_socket(sock), m_parent(parent),
        m_hostname(hostname)
{
    m_socket->UpRef();
}

SocketHandler::~SocketHandler()
{
    if (m_socket)
        m_socket->DownRef();
}

bool SocketHandler::SendStringList(QStringList &strlist, bool lock)
{
    if (!m_socket)
        return false;

    VERBOSE(VB_IMPORTANT, "Locking Socket for write");
    if (lock) m_socket->Lock();
    bool res = m_socket->writeStringList(strlist);
    if (lock) m_socket->Unlock();
    VERBOSE(VB_IMPORTANT, "UnLocking Socket from write");

    return res;
}

bool SocketHandler::SendReceiveStringList(QStringList &strlist,
                                          uint min_reply_length)
{
    if (!m_socket)
        return false;

    return m_socket->SendReceiveStringList(strlist, min_reply_length);
}
