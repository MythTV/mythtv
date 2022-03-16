// MythTV
#include "libmythbase/mythlogging.h"
#include "mythmainwindow.h"
#include "platforms/mythscreensaverwayland.h"
#include "platforms/waylandprotocols/idle_inhibit_unstable_v1.h"
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "platforms/waylandprotocols/idle_inhibit_unstable_v1.c"

#define LOC QString("ScreenSaverWayland: ")

MythScreenSaverWayland::MythScreenSaverWayland(QObject* Parent, MythMainWindow *MainWindow)
  : MythScreenSaver(Parent),
    m_window(MainWindow)
{
    connect(m_window, &MythMainWindow::SignalWindowReady, this, &MythScreenSaverWayland::WindowReady);
}

MythScreenSaverWayland::~MythScreenSaverWayland()
{
    if (m_inhibitor)
        zwp_idle_inhibitor_v1_destroy(m_inhibitor);
    if (m_manager)
        zwp_idle_inhibit_manager_v1_destroy(m_manager);
    delete m_device;
}

/*! \brief Signalled when MythMainWindow has completed initialisation.
 *
 * Note that this may be called multiple times if the main window is re-initialised
 * but there should be no need to delete and recreate our Wayland objects as the
 * compositor etc should not change (though this may need confirming for extreme
 * edge cases).
*/
void MythScreenSaverWayland::WindowReady()
{
    if (!m_device)
        m_device = new MythWaylandDevice(m_window);

    if (!(m_device->m_display && m_device->m_surface))
        return;

    if (!m_manager)
    {
        // We're looking for just the inhibit manager interface
        m_registry.insert({ &zwp_idle_inhibit_manager_v1_interface, nullptr });

        // Search for it
        wl_registry* registry = wl_display_get_registry(m_device->m_display);
        wl_registry_add_listener(registry, &MythWaylandExtras::kRegistryListener, &m_registry);
        wl_display_roundtrip(m_device->m_display);

        // Did we find it
        m_manager = static_cast<zwp_idle_inhibit_manager_v1*>(m_registry.at(&zwp_idle_inhibit_manager_v1_interface));
    }

    if (m_manager)
        LOG(VB_GENERAL, LOG_INFO, LOC + "Ready");
    else
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Setup failed (no compositor support?)");
}

void MythScreenSaverWayland::Disable()
{
    if (m_inhibitor || !m_manager)
        return;
    m_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(m_manager, m_device->m_surface);
    LOG(VB_GENERAL, LOG_INFO, LOC + "Screensaver inhibited");
}

void MythScreenSaverWayland::Restore()
{
    if (m_inhibitor)
        zwp_idle_inhibitor_v1_destroy(m_inhibitor);
    m_inhibitor = nullptr;
    LOG(VB_GENERAL, LOG_INFO, LOC + "Uninhibited screensaver");
}

void MythScreenSaverWayland::Reset()
{
    Restore();
}

bool MythScreenSaverWayland::Asleep()
{
    return false;
}
