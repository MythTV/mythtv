// Qt
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QString>

// MythTV
#include "libmythbase/mythlogging.h"
#include "platforms/mythscreensaverdbus.h"

// Std
#include <array>
#include <cstdint>
#include <string>

#define LOC QString("ScreenSaverDBus: ")

const std::string kApp         = "MythTV";
const std::string kReason      = "Watching TV";
const std::string kDbusInhibit = "Inhibit";

static constexpr size_t NUM_DBUS_METHODS { 4 };
// Thanks to vlc for the set of dbus services to use.
const std::array<const QString,NUM_DBUS_METHODS> kDbusService {
    "org.freedesktop.ScreenSaver", /**< KDE >= 4 and GNOME >= 3.10 */
    "org.freedesktop.PowerManagement.Inhibit", /**< KDE and GNOME <= 2.26 */
    "org.mate.SessionManager", /**< >= 1.0 */
    "org.gnome.SessionManager", /**< GNOME 2.26..3.4 */
};

const std::array<const QString,NUM_DBUS_METHODS> kDbusPath {
    "/ScreenSaver",
    "/org/freedesktop/PowerManagement",
    "/org/mate/SessionManager",
    "/org/gnome/SessionManager",
};

// Service name is also the interface name in all cases

const std::array<const QString,NUM_DBUS_METHODS> kDbusUnInhibit {
    "UnInhibit",
    "UnInhibit",
    "Uninhibit",
    "Uninhibit",
};

class ScreenSaverDBusPrivate
{
    friend class MythScreenSaverDBus;

  public:
    ScreenSaverDBusPrivate(const QString &dbusService, const QString& dbusPath,
                           const QString &dbusInterface, QDBusConnection *bus)
      : m_bus(bus),
        m_interface(new QDBusInterface(dbusService, dbusPath , dbusInterface, *m_bus)),
        m_serviceUsed(dbusService)
    {
        if (!m_interface->isValid())
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Could not connect to dbus service %1: %2")
                .arg(dbusService, m_interface->lastError().message()));
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Created for DBus service: " + dbusService);
        }
    }
    ~ScreenSaverDBusPrivate()
    {
        delete m_interface;
    }
    void Inhibit(QString* errout = nullptr)
    {
        if (m_interface->isValid())
        {
            // method uint org.freedesktop.ScreenSaver.Inhibit(QString application_name, QString reason_for_inhibit)
            QDBusMessage msg = m_interface->call(QDBus::Block,
                                                 kDbusInhibit.c_str(),
                                                 kApp.c_str(), kReason.c_str());
            if (msg.type() == QDBusMessage::ReplyMessage)
            {
                QList<QVariant> replylist = msg.arguments();
                const QVariant& reply = replylist.first();
                m_cookie = reply.toUInt();
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("Successfully inhibited screensaver via %1. cookie %2. nom nom")
                    .arg(m_serviceUsed).arg(m_cookie));
                return;
            }

            // msg.type() == QDBusMessage::ErrorMessage
            if (errout != nullptr)
            {
                *errout = msg.errorMessage();
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Failed to disable screensaver via %1: %2")
                    .arg(m_serviceUsed, msg.errorMessage()));
            }
        }
    }
    void UnInhibit()
    {
        if (m_interface->isValid())
        {
            // Don't block waiting for the reply, there isn't one
            // method void org.freedesktop.ScreenSaver.UnInhibit(uint cookie) (or equivalent)
            if (m_cookie != 0) {
                m_interface->call(QDBus::NoBlock, m_unInhibit , m_cookie);
                m_cookie = 0;
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Screensaver uninhibited via %1")
                    .arg(m_serviceUsed));
            }
        }
    }
    void SetUnInhibit(const QString &method) { m_unInhibit = method; }
    bool isValid() const { return m_interface ? m_interface->isValid() : false; };

  protected:
    uint32_t        m_cookie     {0};
    QDBusConnection *m_bus       {nullptr};
    QDBusInterface  *m_interface {nullptr};
  private:
    // Disable copying and assignment
    Q_DISABLE_COPY(ScreenSaverDBusPrivate)
    QString         m_unInhibit;
    QString         m_serviceUsed;
};

MythScreenSaverDBus::MythScreenSaverDBus(QObject *Parent)
  : MythScreenSaver(Parent),
    m_bus(QDBusConnection::sessionBus())
{
    // service, path, interface, bus - note that interface = service, hence it is used twice
    for (size_t i=0; i < NUM_DBUS_METHODS; i++)
    {
        auto *ssdbp = new ScreenSaverDBusPrivate(kDbusService[i], kDbusPath[i], kDbusService[i], &m_bus);
        if (!ssdbp->isValid())
        {
            delete ssdbp;
            continue;
        }
        ssdbp->SetUnInhibit(kDbusUnInhibit[i]);

        // Possible control. Does it work without error?
        QString dbuserr;
        ssdbp->Inhibit(&dbuserr);
        ssdbp->UnInhibit();
        if (!dbuserr.isEmpty())
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC +
                QString("Error testing disable screensaver via %1: %2")
                .arg(kDbusService[i], dbuserr));
            delete ssdbp;
            continue;
        }
        m_dbusPrivateInterfaces.push_back(ssdbp);
    }
}

MythScreenSaverDBus::~MythScreenSaverDBus()
{
    MythScreenSaverDBus::Restore();
    for (auto * interface : std::as_const(m_dbusPrivateInterfaces))
        delete interface;
}

void MythScreenSaverDBus::Disable()
{
    for (auto * interface : std::as_const(m_dbusPrivateInterfaces))
        interface->Inhibit();
}

void MythScreenSaverDBus::Restore()
{
    for (auto * interface : std::as_const(m_dbusPrivateInterfaces))
        interface->UnInhibit();
}

void MythScreenSaverDBus::Reset()
{
}

bool MythScreenSaverDBus::Asleep()
{
    return false;
}
