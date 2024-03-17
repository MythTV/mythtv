#ifndef MYTHINPUTDEVICEHANDLER_H
#define MYTHINPUTDEVICEHANDLER_H

// Qt
#include <QObject>

// MythTV
#ifdef USING_LIBCEC
#include "devices/mythcecadapter.h"
#endif

class LIRC;
class MythMainWindow;
class JoystickMenuThread;
class AppleRemote;
class AppleRemoteListener;

class MythInputDeviceHandler : public QObject
{
    Q_OBJECT

  public:
    explicit MythInputDeviceHandler(MythMainWindow* Parent);
   ~MythInputDeviceHandler() override;

    void Start           (void);
    void Stop            (bool Finishing = true);
    void Reset           (void);
    void Event           (QEvent *Event) const;
    void Action          (const QString &Action);
    void IgnoreKeys      (bool Ignore);

  public slots:
    void MainWindowReady (void);
    void customEvent     (QEvent *Event) override;

  private:
    Q_DISABLE_COPY(MythInputDeviceHandler)

    MythMainWindow* m_parent     { nullptr };
    bool            m_ignoreKeys { false   };

#ifdef USE_LIRC
    LIRC           *m_lircThread { nullptr };
#endif

#ifdef USING_LIBCEC
    MythCECAdapter  m_cecAdapter;
#endif

#ifdef USE_JOYSTICK_MENU
    JoystickMenuThread  *m_joystickThread { nullptr };
#endif

#ifdef USING_APPLEREMOTE
    AppleRemoteListener *m_appleRemoteListener  { nullptr };
    AppleRemote         *m_appleRemote          { nullptr };
#endif
};

#endif
