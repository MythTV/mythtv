#ifndef MYTH_SCREENSAVER_DBUS_H
#define MYTH_SCREENSAVER_DBUS_H

#include "screensaver.h"

class ScreenSaverDBus : public ScreenSaverControl
{
  public:
    ScreenSaverDBus();
    ~ScreenSaverDBus();

    void Disable(void);
    void Restore(void);
    void Reset(void);

    bool Asleep(void);
  protected:
    class ScreenSaverDBusPrivate *d;
};

#endif // MYTH_SCREENSAVER_DBUS_H

