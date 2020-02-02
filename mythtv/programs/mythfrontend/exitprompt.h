// MythTV
#include "mythpower.h"

class ExitPrompter : public QObject
{
    Q_OBJECT

  public:
    ExitPrompter();
   ~ExitPrompter() override;

  public slots:
    void DoQuit(void);
    void DoHalt(bool Confirmed = true);
    void DoReboot(bool Confirmed = true);
    void DoStandby(void);
    void DoSuspend(bool Confirmed = true);
    void HandleExit(void);
    void ConfirmHalt(void);
    void ConfirmReboot(void);
    void ConfirmSuspend(void);
    void Confirm(MythPower::Feature Action);

  private:
    MythPower* m_power { nullptr };
    QString    m_haltCommand;
    QString    m_rebootCommand;
    QString    m_suspendCommand;
};
