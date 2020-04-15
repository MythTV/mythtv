// MythTV
#include "mythpower.h"

class ExitPrompter : public QObject
{
    Q_OBJECT

  public:
    ExitPrompter();
   ~ExitPrompter() override;

  public slots:
    static void DoQuit(void);
    void DoHalt(bool Confirmed = true);
    void DoReboot(bool Confirmed = true);
    static void DoStandby(void);
    void DoSuspend(bool Confirmed = true);
    void HandleExit(void);
    void ConfirmHalt(void) const;
    void ConfirmReboot(void) const;
    void ConfirmSuspend(void) const;
    void Confirm(MythPower::Feature Action) const;

  private:
    MythPower* m_power { nullptr };
    QString    m_haltCommand;
    QString    m_rebootCommand;
    QString    m_suspendCommand;
};
