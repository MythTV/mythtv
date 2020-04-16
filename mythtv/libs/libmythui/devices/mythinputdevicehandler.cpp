// MythTV
#include "mythlogging.h"
#include "mythinputdevicehandler.h"

#define LOC QString("InputHandler: ")

/*! \class MythInputDeviceHandler
 *
 * \brief A wrapper around sundry external input devices.
 *
 * \todo This could be better implemented with a framework/standardised API
 * and factory methods to simplify adding new devices.
*/
MythInputDeviceHandler::MythInputDeviceHandler(QWidget *Parent)
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
}

void MythInputDeviceHandler::Stop(bool Finishing /* = true */)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Stopping");

#ifdef USING_LIBCEC
    if (Finishing)
        m_cecAdapter.Close();
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

void MythInputDeviceHandler::customEvent(QEvent* /*Event*/)
{
}
