#ifndef MYTH_SCREENSAVER_X11_H
#define MYTH_SCREENSAVER_X11_H

// Qt
#include <QObject>

// MythTV
#include "mythscreensaver.h"

class MythScreenSaverX11 : public QObject, public MythScreenSaver
{
    Q_OBJECT

  public:
    MythScreenSaverX11();
    ~MythScreenSaverX11() override;

    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;

  public slots:
    void ResetSlot();

  protected:
    class ScreenSaverX11Private* m_priv { nullptr };
};

#endif

