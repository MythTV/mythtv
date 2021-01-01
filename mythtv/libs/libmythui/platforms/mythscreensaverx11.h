#ifndef MYTH_SCREENSAVER_X11_H
#define MYTH_SCREENSAVER_X11_H

// Qt
#include <QObject>

// MythTV
#include "mythscreensaver.h"

class MythScreenSaverX11 : public MythScreenSaver
{
    Q_OBJECT

  public:
    explicit MythScreenSaverX11(QObject* Parent);
    ~MythScreenSaverX11() override;

  public slots:
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;
    void ResetSlot();

  protected:
    class ScreenSaverX11Private* m_priv { nullptr };

  private:
    Q_DISABLE_COPY(MythScreenSaverX11)
};

#endif

