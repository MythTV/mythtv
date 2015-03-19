#include <stdint.h>
#include "screensaver-dbus.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QString>

#include "mythlogging.h"

#define LOC                 QString("ScreenSaverDBus: ")

class ScreenSaverDBusPrivate
{
    friend class    ScreenSaverDBus;

  private:
    const QString   m_app           = "MythTV";
    const QString   m_reason        = "Watching TV";
    const QString   m_dbusService   = "org.freedesktop.ScreenSaver";
    const QString   m_dbusPath      = "/ScreenSaver";
    const QString   m_dbusInterface = "org.freedesktop.ScreenSaver";
    const QString   m_dbusInhibit   = "Inhibit";
    const QString   m_dbusUnInhibit = "UnInhibit";

  public:
    ScreenSaverDBusPrivate() :
        m_inhibited(false),
        m_cookie(0),
        m_bus(QDBusConnection::sessionBus()),
        m_interface(new QDBusInterface(m_dbusService, m_dbusPath , m_dbusInterface , m_bus))
    {
        if (!m_interface->isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Could not connect to dbus: " +
                m_interface->lastError().message());
        }
    }
    ~ScreenSaverDBusPrivate()
    {
        delete m_interface;
    }
    void Inhibit(void)
    {
        if (m_interface->isValid())
        {
            // method uint org.freedesktop.ScreenSaver.Inhibit(QString application_name, QString reason_for_inhibit)
            QDBusMessage msg = m_interface->call(QDBus::Block, m_dbusInhibit , m_app, m_reason);
            if (msg.type() == QDBusMessage::ReplyMessage)
            {
                QList<QVariant> replylist = msg.arguments();
                QVariant reply = replylist.first();
                m_cookie = reply.toUInt();
                m_inhibited = true;
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("Successfully inhibited screensaver. cookie %1. nom nom")
                    .arg(m_cookie));
            }
            else // msg.type() == QDBusMessage::ErrorMessage
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to disable screensaver: " +
                    msg.errorMessage());
            }
        }
    }
    void UnInhibit(void)
    {
        if (m_interface->isValid())
        {
            // Don't block waiting for the reply, there isn't one
            // method void org.freedesktop.ScreenSaver.UnInhibit(uint cookie)
            m_interface->call(QDBus::NoBlock, m_dbusUnInhibit , m_cookie);
            m_cookie = 0;
            m_inhibited = false;
            LOG(VB_GENERAL, LOG_INFO, LOC + "Screensaver uninhibited");
        }
    }

  protected:
    bool            m_inhibited;
    uint32_t        m_cookie;
    QDBusConnection m_bus;
    QDBusInterface  *m_interface;
};

ScreenSaverDBus::ScreenSaverDBus() :
    d(new ScreenSaverDBusPrivate)
{
}

ScreenSaverDBus::~ScreenSaverDBus()
{
    // Uninhibit the screensaver first
    Restore();
    delete d;
}

void ScreenSaverDBus::Disable(void)
{
    d->Inhibit();
}

void ScreenSaverDBus::Restore(void)
{
    d->UnInhibit();
}

void ScreenSaverDBus::Reset(void)
{
    d->UnInhibit();
}

bool ScreenSaverDBus::Asleep(void)
{
    return !(d->m_inhibited);
}
