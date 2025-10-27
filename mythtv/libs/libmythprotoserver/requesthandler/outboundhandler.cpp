#include <chrono>

#include <QTimer>
#include <QString>
#include <QStringList>

#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsocket.h"

#include "mythsocketmanager.h"
#include "sockethandler.h"
#include "socketrequesthandler.h"

#include "requesthandler/outboundhandler.h"

OutboundRequestHandler::OutboundRequestHandler(void) 
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &OutboundRequestHandler::ConnectToMaster);
}

void OutboundRequestHandler::ConnectToMaster(void)
{
    m_timer.stop();
    if (!DoConnectToMaster())
        m_timer.start(5s);
}

bool OutboundRequestHandler::DoConnectToMaster(void)
{
    if (m_socket)
        m_socket->DecrRef();

    m_socket = new MythSocket(-1, m_parent);

    QString server   = gCoreContext->GetMasterServerIP();
    QString hostname = gCoreContext->GetMasterHostName();
    int port         = MythCoreContext::GetMasterServerPort();

    if (!m_socket->ConnectToHost(server, port))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to connect to master backend.");
        m_socket->DecrRef();
        m_socket = nullptr;
        return false;
    }

#ifndef IGNORE_PROTO_VER_MISMATCH
    if (!m_socket->Validate())
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Unable to confirm protocol version with backend.");
        m_socket->DecrRef();
        m_socket = nullptr;
        return false;
    }
#endif

    if (!AnnounceSocket())
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Announcement to upstream master backend failed.");
        m_socket->DecrRef();
        m_socket = nullptr;
        return false;
    }

    auto *handler = new SocketHandler(m_socket, m_parent, hostname);
    handler->BlockShutdown(true);
    handler->AllowStandardEvents(true);
    handler->AllowSystemEvents(true);
    m_parent->AddSocketHandler(handler); // register socket for reception of events
    handler->DecrRef(); // drop local instance in counter
    handler = nullptr;

    LOG(VB_GENERAL, LOG_NOTICE, "Connected to master backend.");

    return true;
}

void OutboundRequestHandler::connectionClosed(MythSocket *socket)
{
    // connection has closed, trigger an immediate reconnection
    if (socket == m_socket)
        ConnectToMaster();
}

#include "moc_outboundhandler.cpp"
