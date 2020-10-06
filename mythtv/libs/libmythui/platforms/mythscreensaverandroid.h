#ifndef MYTH_SCREENSAVER_ANDROID_H
#define MYTH_SCREENSAVER_ANDROID_H

// Qt
#include <QObject>

// MythTV
#include "mythscreensaver.h"

class MythScreenSaverAndroid : public MythScreenSaver
{
    Q_OBJECT

  public:
    explicit MythScreenSaverAndroid(QObject* Parent);
   ~MythScreenSaverAndroid() override;

  public slots:
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;
};

#endif

