// MythTV
#include "libmythbase/mythpower.h"

class MythDialogBox;

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
    void DoHalt(bool Confirmed);
    void DoHalt() { DoHalt(true); }
    void DoReboot(bool Confirmed);
    void DoReboot() { DoReboot(true); }
    void DoStandby();
    void DoSuspend(bool Confirmed);
    void DoSuspend() { DoSuspend(true); }
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
    MythDialogBox* m_dialog { nullptr };
};
