#ifndef MYTHSCREENSAVERDRM_H
#define MYTHSCREENSAVERDRM_H

// MythTV
#include "platforms/mythdisplaydrm.h"
#include "mythscreensaver.h"

// 300 second default timeout
#define DRM_TIMEOUT 5
#define DRM_DPMS QString("DPMS")
#define DRM_ON QString("On")
#define DRM_STANDBY QString("Standby")
#define DRM_DPMS_POLL 5 // poll every 5 seconds

class MythScreenSaverDRM : public QObject, public MythScreenSaver
{
    Q_OBJECT

  public:
    static MythScreenSaverDRM* Create(MythDisplay* mDisplay);
   ~MythScreenSaverDRM() override;

    bool IsValid() const;
    void Disable() override;
    void Restore() override;
    void Reset() override;
    bool Asleep() override;

  public slots:
    void ScreenChanged();
    void timerEvent(QTimerEvent* Event) override;

  private:
    explicit MythScreenSaverDRM(MythDisplay* mDisplay);
    void Init();
    void UpdateDPMS();
    void TurnScreenOnOff(bool On);
    bool            m_valid   { false   };
    MythDisplayDRM* m_display { nullptr };
    MythDRMPtr      m_device  { nullptr };
    bool            m_authenticated { false };
    bool            m_asleep  { false };
    int             m_dpmsTimer { 0 };
    int             m_inactiveTimer { 0 };
    int             m_timeout { DRM_TIMEOUT };
    uint64_t        m_dpmsOn { 0 };
    uint64_t        m_dpmsStandby { 1 };
};

#endif
