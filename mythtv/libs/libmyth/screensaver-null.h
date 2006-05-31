#ifndef MYTH_SCREENSAVER_NULL_H
#define MYTH_SCREENSAVER_NULL_H

#include "screensaver.h"

class ScreenSaverNull : public ScreenSaverControl
{
public:
    ScreenSaverNull();
    ~ScreenSaverNull();

    void Disable(void);
    void Restore(void);
    void Reset(void);

    bool Asleep(void);
};

#endif // MYTH_SCREENSAVER_NULL_H

