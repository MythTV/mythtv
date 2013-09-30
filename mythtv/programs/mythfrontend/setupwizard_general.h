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

    GeneralSetupWizard(MythScreenStack *parent, const char *name = 0);
    ~GeneralSetupWizard();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void save(void);

  public slots:
    void OnSubmitPromptReturn(bool submit);
    void OnDeletePromptReturn(bool submit);

  private:
    void CreateBusyDialog(QString message);

    MythUIButton     *m_submitButton;
    MythUIButton     *m_viewButton;
    MythUIButton     *m_deleteButton;

    MythUIButton     *m_nextButton;
    MythUIButton     *m_cancelButton;

    MythUIText       *m_profileLocation;
    MythUIText       *m_adminPassword;

    MythScreenStack  *m_popupStack;
    MythUIBusyDialog *m_busyPopup;

    HardwareProfile  *m_hardwareProfile;

  private slots:
    void loadData(void);
    void slotNext(void);

    void slotSubmit(void);
    void slotView(void);
    void slotDelete(void);
};

#endif

