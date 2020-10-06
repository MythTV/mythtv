#ifndef MYTHSCREENSAVERWAYLAND_H
#define MYTHSCREENSAVERWAYLAND_H

// MythTV
#include "mythscreensaver.h"

class MythScreenSaverWayland : public MythScreenSaver
{
    Q_OBJECT

  public:
    explicit MythScreenSaverWayland(QObject* Parent);

  public slots:
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;
};

#endif
