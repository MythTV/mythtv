#ifndef MYTH_SCREENSAVER_DBUS_H
#define MYTH_SCREENSAVER_DBUS_H

// Qt
#include "QDBusConnection"

// MythTV
#include "mythscreensaver.h"

class MythScreenSaverDBus : public MythScreenSaver
{
    Q_OBJECT

  public:
    MythScreenSaverDBus(QObject* Parent);
   ~MythScreenSaverDBus() override;

  public slots:
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;

  protected:
    QDBusConnection m_bus;
    class ScreenSaverDBusPrivate* m_priv { nullptr };
    QList<ScreenSaverDBusPrivate*> m_dbusPrivateInterfaces;
};

#endif

