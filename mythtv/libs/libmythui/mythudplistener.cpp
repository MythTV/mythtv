#include <QCoreApplication>
#include <QUdpSocket>
#include <QDomDocument>

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythudplistener.h"

#define LOC QString("UDPListener: ")

MythUDPListener::MythUDPListener()
{
    uint udp_port = gCoreContext->GetNumSetting("UDPNotifyPort", 0);
    m_socket = new QUdpSocket(this);
    connect(m_socket, SIGNAL(readyRead()),
            this,     SLOT(ReadPending()));
    if (m_socket->bind(gCoreContext->MythHostAddressAny(), udp_port))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + 
            QString("bound to port %1").arg(udp_port));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("failed to bind to port %1").arg(udp_port));
    }
}

void MythUDPListener::deleteLater(void)
{
    TeardownAll();
    disconnect();
    QObject::deleteLater();
}

void MythUDPListener::TeardownAll(void)
{
    if (!m_socket)
        return;

    LOG(VB_GENERAL, LOG_INFO, LOC + "Disconnecting");

    m_socket->disconnect();
    m_socket->close();
    m_socket->deleteLater();
    m_socket = NULL;
}

void MythUDPListener::ReadPending(void)
{
    QByteArray buf;
    while (m_socket->hasPendingDatagrams())
    {
        buf.resize(m_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(buf.data(), buf.size(),
                               &sender, &senderPort);

        Process(buf);
    }
}

void MythUDPListener::Process(const QByteArray &buf)
{
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
    QDomDocument doc;
    if (!doc.setContent(buf, false, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Parsing xml:\n\t\t\t at line: %1  column: %2\n\t\t\t%3")
                .arg(errorLine).arg(errorColumn).arg(errorMsg));

        return;
    }

    QDomElement docElem = doc.documentElement();
    if (!docElem.isNull())
    {
        if (docElem.tagName() != "mythmessage")
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unknown UDP packet (not <mythmessage> XML)");
            return;
        }

        QString version = docElem.attribute("version", "");
        if (version.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "<mythmessage> missing 'version' attribute");
            return;
        }
    }

    QString msg  = QString("");
    uint timeout = 0;

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "text")
                msg = e.text();
            else if (e.tagName() == "timeout")
                timeout = e.text().toUInt();
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown element: %1")
                    .arg(e.tagName()));
                return;
            }
        }
        n = n.nextSibling();
    }

    if (!msg.isEmpty())
    {
        if (timeout < 0 || timeout > 1000)
            timeout = 0;
        LOG(VB_GENERAL, LOG_INFO, QString("Received message '%1', timeout %2")
            .arg(msg).arg(timeout));
        QStringList args;
        args << QString::number(timeout);
        MythMainWindow *window = GetMythMainWindow();
        MythEvent* me = new MythEvent(MythEvent::MythUserMessage, msg, args);
        qApp->postEvent(window, me);
    }
}
