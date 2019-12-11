#ifndef MYTH_SCREENSAVER_DBUS_H
#define MYTH_SCREENSAVER_DBUS_H

#include "screensaver.h"
#include "QDBusConnection"

class ScreenSaverDBus : public ScreenSaver
{
  public:
    ScreenSaverDBus();
    ~ScreenSaverDBus();

    void Disable(void) override; // ScreenSaver
    void Restore(void) override; // ScreenSaver
    void Reset(void) override; // ScreenSaver

    bool Asleep(void) override; // ScreenSaver
  protected:
    QDBusConnection m_bus;
    class ScreenSaverDBusPrivate *d {nullptr}; // NOLINT(readability-identifier-naming)
    QList<ScreenSaverDBusPrivate *> m_dbusPrivateInterfaces;
};

#endif // MYTH_SCREENSAVER_DBUS_H

