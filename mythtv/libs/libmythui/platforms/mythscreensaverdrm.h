#ifndef MYTHSCREENSAVERDRM_H
#define MYTHSCREENSAVERDRM_H

// MythTV
#include "libmythui/mythscreensaver.h"
#include "libmythui/platforms/mythdisplaydrm.h"

class MythScreenSaverDRM : public MythScreenSaver
{
    Q_OBJECT

  public:
    static MythScreenSaverDRM* Create(QObject* Parent, MythDisplay* mDisplay);

  public slots:
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;

  private:
    explicit MythScreenSaverDRM(QObject* Parent, MythDisplay* mDisplay);
    bool            m_valid   { false   };
    MythDisplayDRM* m_display { nullptr };
    MythDRMPtr      m_device  { nullptr };

};

#endif
