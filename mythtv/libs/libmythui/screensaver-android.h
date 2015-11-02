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

    void Disable(void);
    void Restore(void);
    void Reset(void);

    bool Asleep(void);

};

#endif // MYTH_SCREENSAVER_ANDROID_H

