// Qt
#include <QCoreApplication>
#include <QKeyEvent>
#include <QDir>

// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"

#include "mythmainwindow.h"
#include "mythinputdevicehandler.h"

#if CONFIG_JOYSTICK_MENU
#include "devices/jsmenu.h"
#include "devices/jsmenuevent.h"
#endif

#if CONFIG_APPLEREMOTE
#include "devices/AppleRemoteListener.h"
#endif

#if CONFIG_LIRC
#include "devices/lirc.h"
#endif

#if defined CONFIG_LIRC || CONFIG_APPLEREMOTE
#include "devices/lircevent.h"
#endif

#define LOC QString("InputHandler: ")

/*! \class MythInputDeviceHandler
 *
 * \brief A wrapper around sundry external input devices.
 *
 * \todo This could be better implemented with a framework/standardised API
 * and factory methods to simplify adding new devices.
*/
MythInputDeviceHandler::MythInputDeviceHandler(MythMainWindow *Parent)
  : m_parent(Parent)
{
    MythInputDeviceHandler::Start();
}

MythInputDeviceHandler::~MythInputDeviceHandler()
{
    MythInputDeviceHandler::Stop();
}

void MythInputDeviceHandler::Start(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Starting");

#if CONFIG_LIRC
    if (!m_lircThread)
    {
        QString config = GetConfDir() + "/lircrc";
        if (!QFile::exists(config))
            config = QDir::homePath() + "/.lircrc";

        // lircd socket moved from /dev/ to /var/run/lirc/ in lirc 0.8.6
        QString socket = "/dev/lircd";
        if (!QFile::exists(socket))
            socket = "/var/run/lirc/lircd";

        m_lircThread = new LIRC(this, GetMythDB()->GetSetting("LircSocket", socket), "mythtv", config);
        if (m_lircThread->Init())
        {
            m_lircThread->start();
        }
        else
        {
            m_lircThread->deleteLater();
            m_lircThread = nullptr;
        }
    }
#endif

#if CONFIG_JOYSTICK_MENU
    if (!m_joystickThread)
    {
        QString config = GetConfDir() + "/joystickmenurc";
        m_joystickThread = new JoystickMenuThread(this);
        if (m_joystickThread->Init(config))
            m_joystickThread->start();
    }
#endif

#if CONFIG_APPLEREMOTE
    if (!m_appleRemoteListener)
    {
        m_appleRemoteListener = new AppleRemoteListener(this);
        m_appleRemote         = AppleRemote::Get();

        m_appleRemote->setListener(m_appleRemoteListener);
        m_appleRemote->startListening();
        if (m_appleRemote->isListeningToRemote())
        {
            m_appleRemote->start();
        }
        else
        {
            // start listening failed, no remote receiver present
            delete m_appleRemote;
            delete m_appleRemoteListener;
            m_appleRemote = nullptr;
            m_appleRemoteListener = nullptr;
        }
    }
#endif
}

void MythInputDeviceHandler::Stop([[maybe_unused]] bool Finishing /* = true */)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Stopping");

#if CONFIG_LIBCEC
    if (Finishing)
        m_cecAdapter.Close();
#endif

#if CONFIG_APPLEREMOTE
    if (Finishing)
    {
        delete m_appleRemote;
        delete m_appleRemoteListener;
        m_appleRemote = nullptr;
        m_appleRemoteListener = nullptr;
    }
#endif

#if CONFIG_JOYSTICK_MENU
    if (m_joystickThread && Finishing)
    {
        if (m_joystickThread->isRunning())
        {
            m_joystickThread->Stop();
            m_joystickThread->wait();
        }
        delete m_joystickThread;
        m_joystickThread = nullptr;
    }
#endif

#if CONFIG_LIRC
    if (m_lircThread)
    {
        m_lircThread->deleteLater();
        m_lircThread = nullptr;
    }
#endif
}

void MythInputDeviceHandler::Reset(void)
{
    Stop(false);
    Start();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void MythInputDeviceHandler::Event(QEvent *Event) const
{
    if (!Event)
        return;

#if CONFIG_APPLEREMOTE
    if (m_appleRemote)
    {
        if (Event->type() == QEvent::WindowActivate)
            m_appleRemote->startListening();
        if (Event->type() == QEvent::WindowDeactivate)
            m_appleRemote->stopListening();
    }
#endif
}

void MythInputDeviceHandler::Action([[maybe_unused]] const QString &Action)
{
#if CONFIG_LIBCEC
    m_cecAdapter.Action(Action);
#endif
}

void MythInputDeviceHandler::IgnoreKeys(bool Ignore)
{
    if (Ignore)
        LOG(VB_GENERAL, LOG_INFO, LOC + "Locking input devices");
    else
        LOG(VB_GENERAL, LOG_INFO, LOC + "Unlocking input devices");
    m_ignoreKeys = Ignore;
#if CONFIG_LIBCEC
    m_cecAdapter.IgnoreKeys(Ignore);
#endif
}

void MythInputDeviceHandler::MainWindowReady(void)
{
#if CONFIG_LIBCEC
    // Open any adapter after the window has been created to ensure we capture
    // the EDID if available - and hence get a more accurate Physical Address.
    // This will close any existing adapter in the event that the window has been re-init'ed.
    m_cecAdapter.Open(m_parent);
#endif
}

void MythInputDeviceHandler::customEvent([[maybe_unused]] QEvent* Event)
{
    if (m_ignoreKeys)
        return;

    QScopedPointer<QKeyEvent> key { new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier) };
    QObject* target = nullptr;
    QString error;

#if CONFIG_JOYSTICK_MENU
    if (Event->type() == JoystickKeycodeEvent::kEventType)
    {
        auto *jke = dynamic_cast<JoystickKeycodeEvent *>(Event);
        if (!jke)
            return;

        int keycode = jke->key();
        if (keycode)
        {
            key.reset(new QKeyEvent(jke->keyAction(), keycode, jke->keyModifiers()));
            target = m_parent->GetTarget(*key);
        }
        else
        {
            error = jke->getJoystickMenuText();
        }
    }
#endif

#if CONFIG_LIRC || CONFIG_APPLEREMOTE
    if (Event->type() == LircKeycodeEvent::kEventType)
    {
        auto *lke = dynamic_cast<LircKeycodeEvent *>(Event);
        if (!lke)
            return;

        if (LircKeycodeEvent::kLIRCInvalidKeyCombo == lke->modifiers())
        {
            error = lke->lirctext();
        }
        else
        {
            key.reset(new QKeyEvent(lke->keytype(), lke->key(), lke->modifiers(), lke->text()));
            target = m_parent->GetTarget(*key);
        }
    }
#endif

    if (!error.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Attempt to convert key sequence '%1' to a Qt key sequence failed.").arg(error));
    }
    else if (target)
    {
        MythMainWindow::ResetScreensaver();
        if (MythMainWindow::IsScreensaverAsleep())
            return;
        QCoreApplication::sendEvent(target, key.data());
    }
}

#include "moc_mythinputdevicehandler.cpp"
