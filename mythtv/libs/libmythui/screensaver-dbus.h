#ifndef MYTH_SCREENSAVER_DBUS_H
#define MYTH_SCREENSAVER_DBUS_H

#include "screensaver.h"
#include "QDBusConnection"

class ScreenSaverDBus : public ScreenSaver
{
  public:
    ScreenSaverDBus();
    ~ScreenSaverDBus();

    void Disable(void);
    void Restore(void);
    void Reset(void);

    bool Asleep(void);
  protected:
    QDBusConnection m_bus;
    class ScreenSaverDBusPrivate *d;
    QList<ScreenSaverDBusPrivate *> m_dbusPrivateInterfaces;
};

#endif // MYTH_SCREENSAVER_DBUS_H

