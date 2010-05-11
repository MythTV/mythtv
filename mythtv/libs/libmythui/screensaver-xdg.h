#ifndef MYTH_SCREENSAVER_XDG_H
#define MYTH_SCREENSAVER_XDG_H

#include <QObject>

#include "screensaver.h"

class ScreenSaverXDG : public QObject, public ScreenSaverControl
{
    Q_OBJECT

  public:
    ScreenSaverXDG();
    ~ScreenSaverXDG();

    void Disable(void);
    void Restore(void);
    void Reset(void);

    bool Asleep(void);

    // determine if xdg-utils is installed
    static bool IsXDGInstalled(void);

  protected:
    class ScreenSaverXDGPrivate *d;
};

#endif // MYTH_SCREENSAVER_XDG_H

