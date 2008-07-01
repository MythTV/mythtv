#ifndef MYTH_SCREENSAVER_X11_H
#define MYTH_SCREENSAVER_X11_H

#include <QObject>

#include "screensaver.h"

class ScreenSaverX11 : public QObject, public ScreenSaverControl
{
    Q_OBJECT

  public:
    ScreenSaverX11();
    ~ScreenSaverX11();

    void Disable(void);
    void Restore(void);
    void Reset(void);

    bool Asleep(void);

  public slots:
    void resetSlot();

  protected:
    class ScreenSaverX11Private *d;
};

#endif // MYTH_SCREENSAVER_X11_H

