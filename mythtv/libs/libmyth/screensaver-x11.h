#ifndef MYTH_SCREENSAVER_X11_H
#define MYTH_SCREENSAVER_X11_H

#include "screensaver.h"

class ScreenSaverX11 : public ScreenSaverControl
{
public:
    ScreenSaverX11();
    ~ScreenSaverX11();

    void Disable(void);
    void Restore(void);
    void Reset(void);

protected:
    class ScreenSaverX11Private *d;
};

#endif // MYTH_SCREENSAVER_X11_H

