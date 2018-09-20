#ifndef MYTH_SCREENSAVER_NULL_H
#define MYTH_SCREENSAVER_NULL_H

#include "screensaver.h"

class ScreenSaverNull : public ScreenSaver
{
public:
    ScreenSaverNull() = default;
    ~ScreenSaverNull() = default;

    void Disable(void);
    void Restore(void);
    void Reset(void);

    bool Asleep(void);
};

#endif // MYTH_SCREENSAVER_NULL_H

