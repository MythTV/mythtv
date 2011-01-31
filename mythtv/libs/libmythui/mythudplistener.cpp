#include <QCoreApplication>
#include <QUdpSocket>
#include <QDomDocument>

#include "mythcorecontext.h"
#include "mythverbose.h"
#include "mythmainwindow.h"
#include "mythudplistener.h"

#define LOC QString("UDPListener: ")
#define ERR QString("UPDListener Error: ")

MythUDPListener::MythUDPListener()
{
    uint udp_port = gCoreContext->GetNumSetting("UDPNotifyPort", 0);
    m_socket = new QUdpSocket(this);
    connect(m_socket, SIGNAL(readyRead()),
            this,     SLOT(ReadPending()));
    if (m_socket->bind(udp_port))
    {
        VERBOSE(VB_GENERAL, LOC +  QString("bound to port %1").arg(udp_port));
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC +
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

    VERBOSE(VB_GENERAL, LOC + "Disconnecting");

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
        VERBOSE(VB_IMPORTANT, ERR +
            QString("Parsing xml:\n\t\t\t at line: %1  column: %2\n\t\t\t%3")
            .arg(errorLine).arg(errorColumn).arg(errorMsg));

        return;
    }

    QDomElement docElem = doc.documentElement();
    if (!docElem.isNull())
    {
        if (docElem.tagName() != "mythmessage")
        {
            VERBOSE(VB_IMPORTANT, ERR +
                "Unknown UDP packet (not <mythmessage> XML)");
            return;
        }

        QString version = docElem.attribute("version", "");
        if (version.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, ERR +
                "<mythmessage> missing 'version' attribute");
            return;
        }
    }

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "text")
            {
                QString msg = e.text();
                VERBOSE(VB_GENERAL, LOC + msg);
                MythMainWindow *window = GetMythMainWindow();
                MythEvent* me = new MythEvent(MythEvent::MythUserMessage, msg);
                qApp->postEvent(window, me);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, ERR + QString("Unknown element: %1")
                    .arg(e.tagName()));
                return;
            }
        }
        n = n.nextSibling();
    }
}
