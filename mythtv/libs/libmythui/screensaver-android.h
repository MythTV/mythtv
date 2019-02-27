#ifndef MYTH_SCREENSAVER_ANDROID_H
#define MYTH_SCREENSAVER_ANDROID_H

#include <QObject>

#include "screensaver.h"

class ScreenSaverAndroid : public QObject, public ScreenSaver
{
    Q_OBJECT

  public:
    ScreenSaverAndroid();
    ~ScreenSaverAndroid();

    void Disable(void) override; // ScreenSaver
    void Restore(void) override; // ScreenSaver
    void Reset(void) override; // ScreenSaver

    bool Asleep(void) override; // ScreenSaver

};

#endif // MYTH_SCREENSAVER_ANDROID_H

