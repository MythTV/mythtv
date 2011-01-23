#ifndef GENERALSETUPWIZARD_H
#define GENERALSETUPWIZARD_H

#include <uitypes.h>
#include <mythwidgets.h>
#include <mythdialogs.h>

// libmythui
#include <mythuibutton.h>
#include <mythuicheckbox.h>
#include <mythuibuttonlist.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>

class GeneralSetupWizard : public MythScreenType
{
  Q_OBJECT

  public:

    GeneralSetupWizard(MythScreenStack *parent, const char *name = 0);
    ~GeneralSetupWizard();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void save(void);

  private:
    MythUIButtonList    *m_ldateButtonList;
    MythUIButtonList    *m_sdateButtonList;
    MythUIButtonList    *m_timeButtonList;

    MythUIButton        *m_nextButton;
    MythUIButton        *m_cancelButton;

  private slots:
    void loadData(void);
    void slotNext(void);
};

#endif

