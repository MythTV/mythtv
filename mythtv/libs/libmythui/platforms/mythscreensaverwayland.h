#ifndef MYTHSCREENSAVERWAYLAND_H
#define MYTHSCREENSAVERWAYLAND_H

// MythTV
#include "platforms/mythwaylandextras.h"
#include "mythscreensaver.h"

class MythMainWindow;
struct zwp_idle_inhibit_manager_v1;
struct zwp_idle_inhibitor_v1;

class MythScreenSaverWayland : public MythScreenSaver
{
    Q_OBJECT

  public:
    MythScreenSaverWayland(QObject* Parent, MythMainWindow* MainWindow);
   ~MythScreenSaverWayland() override;

  public slots:
    void WindowReady();
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;

  private:
    MythMainWindow*              m_window    { nullptr };
    MythWaylandRegistry          m_registry;
    MythWaylandDevice*           m_device    { nullptr };
    zwp_idle_inhibit_manager_v1* m_manager   { nullptr };
    zwp_idle_inhibitor_v1*       m_inhibitor { nullptr };
};

#endif
