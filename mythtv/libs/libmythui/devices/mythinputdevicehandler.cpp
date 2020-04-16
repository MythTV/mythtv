// Qt
#include <QCoreApplication>
#include <QKeyEvent>

// MythTV
#include "mythlogging.h"
#include "mythdirs.h"
#include "mythuihelper.h"
#include "mythmainwindow.h"
#include "mythinputdevicehandler.h"

#ifdef USE_JOYSTICK_MENU
#include "jsmenu.h"
#include "jsmenuevent.h"
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

#ifdef USE_JOYSTICK_MENU
    if (!m_joystickThread)
    {
        QString config = GetConfDir() + "/joystickmenurc";
        m_joystickThread = new JoystickMenuThread(this);
        if (m_joystickThread->Init(config))
            m_joystickThread->start();
    }
#endif
}

void MythInputDeviceHandler::Stop(bool Finishing /* = true */)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Stopping");

#ifdef USING_LIBCEC
    if (Finishing)
        m_cecAdapter.Close();
#endif

#ifdef USE_JOYSTICK_MENU
    if (m_joystickThread)
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
}

void MythInputDeviceHandler::Reset(void)
{
    Stop(false);
    Start();
}

void MythInputDeviceHandler::Action(const QString &Action)
{
#ifdef USING_LIBCEC
    m_cecAdapter.Action(Action);
#else
    (void) Action;
#endif
}

void MythInputDeviceHandler::IgnoreKeys(bool Ignore)
{
    if (Ignore)
        LOG(VB_GENERAL, LOG_INFO, LOC + "Locking input devices");
    else
        LOG(VB_GENERAL, LOG_INFO, LOC + "Unlocking input devices");
    m_ignoreKeys = Ignore;
#ifdef USING_LIBCEC
    m_cecAdapter.IgnoreKeys(Ignore);
#endif
}

void MythInputDeviceHandler::MainWindowReady(void)
{
#ifdef USING_LIBCEC
    // Open any adapter after the window has been created to ensure we capture
    // the EDID if available - and hence get a more accurate Physical Address.
    // This will close any existing adapter in the event that the window has been re-init'ed.
    m_cecAdapter.Open();
#endif
}

void MythInputDeviceHandler::customEvent(QEvent* Event)
{
    if (m_ignoreKeys)
        return;

#ifdef USE_JOYSTICK_MENU
    if (Event->type() == JoystickKeycodeEvent::kEventType)
    {
        auto *jke = dynamic_cast<JoystickKeycodeEvent *>(Event);
        if (!jke)
            return;

        int keycode = jke->getKeycode();
        if (keycode)
        {
            MythUIHelper::ResetScreensaver();
            if (GetMythUI()->GetScreenIsAsleep())
                return;

            auto mod = Qt::KeyboardModifiers(keycode & static_cast<int>(Qt::MODIFIER_MASK));
            int k = (keycode & static_cast<int>(~Qt::MODIFIER_MASK)); // trim off the mod
            QString text;
            QKeyEvent key(jke->isKeyDown() ? QEvent::KeyPress :
                                             QEvent::KeyRelease, k, mod, text);

            QObject *target = m_parent->getTarget(key);
            if (!target)
                QCoreApplication::sendEvent(m_parent, &key);
            else
                QCoreApplication::sendEvent(target, &key);
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Attempt to convert '%1' to a key sequence failed. Fix your key mappings.")
                    .arg(jke->getJoystickMenuText()));
        }

        return;
    }
#endif
}
