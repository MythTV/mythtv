#ifndef MYTH_SCREENSAVER_NULL_H
#define MYTH_SCREENSAVER_NULL_H

#include "screensaver.h"

class ScreenSaverNull : public ScreenSaver
{
public:
    ScreenSaverNull() = default;
    ~ScreenSaverNull() = default;

    void Disable(void) override; // ScreenSaver
    void Restore(void) override; // ScreenSaver
    void Reset(void) override; // ScreenSaver

    bool Asleep(void) override; // ScreenSaver
};

#endif // MYTH_SCREENSAVER_NULL_H

