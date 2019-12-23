#ifndef GENERALSETUPWIZARD_H
#define GENERALSETUPWIZARD_H

// libmythui
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythuicheckbox.h"
#include "mythscreentype.h"
#include "mythdialogbox.h"
#include "mythprogressdialog.h"

class HardwareProfile;

class GeneralSetupWizard : public MythScreenType
{
  Q_OBJECT

  public:

    explicit GeneralSetupWizard(MythScreenStack *parent, const char *name = nullptr);
    ~GeneralSetupWizard() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    void save(void);

  public slots:
    void OnSubmitPromptReturn(bool submit);
    void OnDeletePromptReturn(bool submit);

  private:
    void CreateBusyDialog(const QString& message);

    MythUIButton     *m_submitButton    {nullptr};
    MythUIButton     *m_viewButton      {nullptr};
    MythUIButton     *m_deleteButton    {nullptr};

    MythUIButton     *m_nextButton      {nullptr};
    MythUIButton     *m_cancelButton    {nullptr};

    MythUIText       *m_profileLocation {nullptr};
    MythUIText       *m_adminPassword   {nullptr};

    MythScreenStack  *m_popupStack      {nullptr};
    MythUIBusyDialog *m_busyPopup       {nullptr};

    HardwareProfile  *m_hardwareProfile {nullptr};

  private slots:
    void loadData(void);
    void slotNext(void);

    void slotSubmit(void);
    void slotView(void);
    void slotDelete(void);
};

#endif

