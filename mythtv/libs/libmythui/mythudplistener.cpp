// Qt
#include <QCoreApplication>
#include <QDomDocument>
#include <QHostAddress>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "mythmainwindow.h"
#include "mythudplistener.h"

// Std
#include <thread>

#define LOC QString("UDPListener: ")

MythUDPListener::MythUDPListener()
{
    connect(this, &MythUDPListener::EnableUDPListener, this, &MythUDPListener::DoEnable);
}

MythUDPListener::~MythUDPListener()
{
    DoEnable(false);
}

void MythUDPListener::DoEnable(bool Enable)
{
    if (Enable)
    {
        if (m_socketPool)
            return;

        LOG(VB_GENERAL, LOG_INFO, LOC + "Enabling");
        m_socketPool = new ServerPool(this);
        connect(m_socketPool, &ServerPool::newDatagram, this, &MythUDPListener::Process);
        QList<QHostAddress> addrs = ServerPool::DefaultListen();
        addrs << ServerPool::DefaultBroadcast();
        auto port = static_cast<uint16_t>(gCoreContext->GetNumSetting("UDPNotifyPort", 0));
        if (!m_socketPool->bind(addrs, port, false))
        {
            delete m_socketPool;
            m_socketPool = nullptr;
        }
    }
    else
    {
        if (!m_socketPool)
            return;

        LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling");
        m_socketPool->close();
        delete m_socketPool;
        m_socketPool = nullptr;
    }
}

void MythUDPListener::Process(const QByteArray& Buffer, const QHostAddress& /*Sender*/,
                              quint16 /*SenderPort*/)
{
    QString errormsg;
    QDomDocument doc;
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    int line = 0;
    int column = 0;
    if (!doc.setContent(Buffer, false, &errormsg, &line, &column))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error parsing xml: Line: %1 Column: %2 Error: %3")
            .arg(line).arg(column).arg(errormsg));
        return;
    }
#else
    auto parseResult = doc.setContent(Buffer);
    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error parsing xml: Line: %1 Column: %2 Error: %3")
            .arg(parseResult.errorLine).arg(parseResult.errorColumn).arg(parseResult.errorMessage));
        return;
    }
#endif

    auto element = doc.documentElement();
    bool notification = false;
    if (!element.isNull())
    {
        if (element.tagName() != "mythmessage" && element.tagName() != "mythnotification")
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown UDP packet (not <mythmessage> XML)");
            return;
        }

        if (element.tagName() == "mythnotification")
            notification = true;

        if (auto version = element.attribute("version", ""); version.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "<mythmessage> missing 'version' attribute");
            return;
        }
    }

    QString msg;
    std::chrono::seconds timeout = 0s;
    QString image;
    QString origin;
    QString description;
    QString extra;
    QString progress_text;
    float progress = -1.0F;
    bool fullscreen = false;
    bool error = false;
    int visibility = 0;
    QString type = "normal";

    auto node = element.firstChild();
    while (!node.isNull())
    {
        auto dom = node.toElement();
        if (!dom.isNull())
        {
            auto tagname = dom.tagName();
            if (tagname == "text")
                msg = dom.text();
            else if (tagname == "timeout")
                timeout = std::chrono::seconds(dom.text().toUInt());
            else if (notification && tagname == "image")
                image = dom.text();
            else if (notification && tagname == "origin")
                origin = dom.text();
            else if (notification && tagname == "description")
                description = dom.text();
            else if (notification && tagname == "extra")
                extra = dom.text();
            else if (notification && tagname == "progress_text")
                progress_text = dom.text();
            else if (notification && tagname == "fullscreen")
                fullscreen = dom.text().toLower() == "true";
            else if (notification && tagname == "error")
                error = dom.text().toLower() == "true";
            else if (tagname == "visibility")
                visibility = dom.text().toInt();
            else if (tagname == "type")
                type = dom.text();
            else if (notification && tagname == "progress")
            {
                bool ok = false;
                if (progress = dom.text().toFloat(&ok); !ok)
                    progress = -1.0F;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unknown element: %1")
                    .arg(tagname));
            }
        }
        node = node.nextSibling();
    }

    if (!msg.isEmpty() || !image.isEmpty() || !extra.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Received %1 '%2', timeout %3")
            .arg(notification ? "notification" : "message",
                 msg, QString::number(timeout.count())));
        if (timeout > 1000s)
            timeout = notification ? 5s : 0s;
        if (notification)
        {
            origin = origin.isEmpty() ? tr("UDP Listener") : origin;
            ShowNotification(error ? MythNotification::kError :
                             MythNotification::TypeFromString(type),
                             msg, origin, description, image, extra,
                             progress_text, progress, timeout,
                             fullscreen, static_cast<VNMask>(visibility));
        }
        else
        {
            QStringList args(QString::number(timeout.count()));
            qApp->postEvent(GetMythMainWindow(), new MythEvent(MythEvent::kMythUserMessage, msg, args));
        }
    }
}

void MythUDP::EnableUDPListener(bool Enable)
{
    if (Instance().m_thread && Instance().m_listener)
    {
        emit Instance().m_listener->EnableUDPListener(Enable);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "EnableUDPListener called after MythUDPListener instance is deleted");
    }
}

MythUDP& MythUDP::Instance()
{
    static MythUDP s_instance;
    return s_instance;
}

MythUDP::MythUDP()
  : m_listener(new MythUDPListener),
    m_thread(new MThread("UDP"))
{
    m_listener->moveToThread(m_thread->qthread());
    m_thread->start();
    while (!m_thread->qthread()->isRunning())
    {
        std::this_thread::sleep_for(5us);
    }
}

MythUDP::~MythUDP()
{
    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
    }
    delete m_thread;
    delete m_listener;
}

void MythUDP::StopUDPListener()
{
    if (Instance().m_thread)
    {
        Instance().m_thread->quit();
        Instance().m_thread->wait();
        delete Instance().m_thread;
        Instance().m_thread = nullptr;
    }

    delete Instance().m_listener;
    Instance().m_listener = nullptr;
}
