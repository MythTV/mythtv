// Qt
#include <QWidget>
#include <QtGlobal>
#include <QGuiApplication>
// N.B. Private headers
#include <qpa/qplatformnativeinterface.h>

// MythTV
#include "libmythbase/mythlogging.h"
#include "platforms/mythwaylandextras.h"

// Wayland
#include <wayland-client.h>

#define LOC QString("Wayland: ")

/*! \class MythWaylandDevice
 * \brief A simple wrapper to retrieve the major Wayland objects from the Qt
 * Wayland native interface.
 *
 * \note This class requires access to private Qt headers. When processing the
 * libmythui.pro file, qmake will throw out warnings on binary compatibility
 *  - our use of the private headers is however limited here, the API has not
 * changed for some time and appears to be stable into Qt6.
*/
MythWaylandDevice::MythWaylandDevice(QWidget* Widget)
{
    if (!Widget)
        return;

    QWindow* window = Widget->windowHandle();
    if (!window)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to retrieve native window");
        return;
    }

    QPlatformNativeInterface* interface = QGuiApplication::platformNativeInterface();
    m_display    = reinterpret_cast<wl_display*>(interface->nativeResourceForWindow("display", window));
    m_surface    = reinterpret_cast<wl_surface*>(interface->nativeResourceForWindow("surface", window));
    m_compositor = reinterpret_cast<wl_compositor*>(interface->nativeResourceForWindow("compositor", window));

    if (!(m_display && m_surface && m_compositor))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to retrieve Wayland resources");
}

/*! \brief Provide a rendering optimisation hint to the compositor.
 *
 * This is only a hint and support varies by compositor. As a result we still
 * need to use other workarounds to ensure alpha blending works correctly.
 *
 * \sa MythDisplay::ConfigureQtGUI
*/
bool MythWaylandDevice::SetOpaqueRegion(const QRect Region) const
{
    if (!(m_surface && m_compositor))
        return false;

    wl_region* region = wl_compositor_create_region(m_compositor);
    if (!region)
        return false;

    wl_region_add(region, Region.left(), Region.top(), Region.width(), Region.height());
    wl_surface_set_opaque_region(m_surface, region);
    wl_surface_commit(m_surface);
    wl_region_destroy(region);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Set opaque region: %1x%2+%3+%4")
        .arg(Region.width()).arg(Region.height()).arg(Region.left()).arg(Region.top()));
    return true;
}

const struct wl_registry_listener MythWaylandExtras::kRegistryListener = { &MythWaylandExtras::AnnounceGlobal, nullptr };

/*! \brief MythTV implementation of the Wayland registry callback.
 *
 * This attempts to generalise the process of filtering registries but this may
 * not work for all use cases.
*/
void MythWaylandExtras::AnnounceGlobal(void *Opaque, struct wl_registry *Reg,
                                       uint32_t Name, const char *Interface, uint32_t Version)
{
    auto * registry = reinterpret_cast<MythWaylandRegistry*>(Opaque);
    auto found = std::find_if(registry->begin(), registry->end(), [&](const auto IFace)
    {
        return (strcmp(Interface, IFace.first->name) == 0) && (IFace.first->version == static_cast<int>(Version));
    });

    if (found != registry->end())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found interface: %1 (v%2)")
            .arg(found->first->name).arg(found->first->version));
        found->second = wl_registry_bind(Reg, Name, found->first, static_cast<uint32_t>(found->first->version));
        if (!found->second)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to bind %1").arg(found->first->name));
    }
}

/// \brief Check whether we can connect to a Wayland server.
bool MythWaylandDevice::IsAvailable()
{
    static bool s_checked = false;
    static bool s_available = false;
    if (!s_checked)
    {
        s_checked = true;
        auto waylanddisplay = qEnvironmentVariable("WAYLAND_DISPLAY");
        const auto *name = waylanddisplay.isEmpty() ? nullptr : waylanddisplay.toLocal8Bit().constData();
        if (auto *display = wl_display_connect(name); display)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Available (WAYLAND_DISPLAY: '%1')")
                .arg(waylanddisplay));
            s_available = true;
            wl_display_disconnect(display);
        }
    }
    return s_available;
}
