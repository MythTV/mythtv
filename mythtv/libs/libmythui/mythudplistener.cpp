#include <QCoreApplication>
#include <QUdpSocket>
#include <QDomDocument>
#include <QList>
#include <QHostAddress>

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythudplistener.h"

#define LOC QString("UDPListener: ")

MythUDPListener::MythUDPListener() :
    m_socketPool(NULL)
{
    Enable();
}

void MythUDPListener::deleteLater(void)
{
    Disable();
    disconnect();
    QObject::deleteLater();
}

void MythUDPListener::Enable(void)
{
    if (m_socketPool)
        return;

    LOG(VB_GENERAL, LOG_INFO, LOC + "Enabling");

    m_socketPool = new ServerPool(this);
    connect(m_socketPool, SIGNAL(newDatagram(QByteArray, QHostAddress,
                                                quint16)),
            this,         SLOT(Process(const QByteArray, QHostAddress,
                                       quint16)));

    QList<QHostAddress> addrs = ServerPool::DefaultListen();
    addrs << ServerPool::DefaultBroadcast();

    if (!m_socketPool->bind(addrs,
            gCoreContext->GetNumSetting("UDPNotifyPort", 0), false))
    {
        delete m_socketPool;
        m_socketPool = NULL;
    }
}

void MythUDPListener::Disable(void)
{
    if (!m_socketPool)
        return;

    LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling");

    m_socketPool->close();
    delete m_socketPool;
    m_socketPool = NULL;
}

void MythUDPListener::Process(const QByteArray &buf, QHostAddress sender,
                              quint16 senderPort)
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
        if (timeout > 1000)
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
