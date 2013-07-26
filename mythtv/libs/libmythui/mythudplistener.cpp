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
    bool notification = false;
    if (!docElem.isNull())
    {
        if (docElem.tagName() != "mythmessage" &&
            docElem.tagName() != "mythnotification")
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Unknown UDP packet (not <mythmessage> XML)");
            return;
        }

        if (docElem.tagName() == "mythnotification")
        {
            notification = true;
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
    QString image;
    QString origin;
    QString description = "";
    QString extra = "";
    QString progress_text = "";
    float progress = -1.0f;
    bool fullscreen = false;
    bool error = false;
    int visibility = 0;
    QString type = "normal";

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
            else if (notification && e.tagName() == "image")
                image = e.text();
            else if (notification && e.tagName() == "origin")
                origin = e.text();
            else if (notification && e.tagName() == "description")
                description = e.text();
            else if (notification && e.tagName() == "extra")
                extra = e.text();
            else if (notification && e.tagName() == "progress_text")
                progress_text = e.text();
            else if (notification && e.tagName() == "fullscreen")
                fullscreen = e.text().toLower() == "true";
            else if (notification && e.tagName() == "error")
                error = e.text().toLower() == "true";
            else if (e.tagName() == "visibility")
                visibility = e.text().toUInt();
            else if (e.tagName() == "type")
                type = e.text();
            else if (notification && e.tagName() == "progress")
            {
                bool ok;
                progress = e.text().toFloat(&ok);
                if (!ok)
                    progress = -1.0f;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown element: %1")
                    .arg(e.tagName()));
            }
        }
        n = n.nextSibling();
    }

    if (!msg.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Received %1 '%2', timeout %3")
            .arg(notification ? "notification" : "message").arg(msg).arg(timeout));
        if (timeout > 1000)
            timeout = notification ? 5 : 0;
        if (notification)
        {
            origin = origin.isNull() ? tr("UDP Listener") : origin;
            ShowNotification(error ? MythNotification::Error :
                                     MythNotification::TypeFromString(type),
                             msg, origin, description, image, extra,
                             progress_text, progress, timeout, fullscreen,
                             visibility);
        }
        else
        {
            QStringList args;
            args << QString::number(timeout);
            MythMainWindow *window = GetMythMainWindow();
            MythEvent* me = new MythEvent(MythEvent::MythUserMessage, msg, args);
            qApp->postEvent(window, me);
        }
    }
}
