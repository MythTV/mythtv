// MythTV
#include "mythpower.h"

class ExitPrompter : public QObject
{
    Q_OBJECT

  public:
    ExitPrompter();
   ~ExitPrompter() override;

  public slots:
    void HandleExit();

  protected slots:
    void DoQuit();
    void DoHalt(bool Confirmed = true);
    void DoReboot(bool Confirmed = true);
    void DoStandby();
    void DoSuspend(bool Confirmed = true);
    void ConfirmHalt();
    void ConfirmReboot();
    void ConfirmSuspend();
    void Confirm(MythPower::Feature Action);
    void MainDialogClosed(const QString& /*unused*/, int Id);

  private:
    MythPower* m_power { nullptr };
    bool       m_confirm { true };
    QString    m_haltCommand;
    QString    m_rebootCommand;
    QString    m_suspendCommand;
};
