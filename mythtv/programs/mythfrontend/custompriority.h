#ifndef CUSTOMPRIORITY_H_
#define CUSTOMPRIORITY_H_

#include "programinfo.h"

#include "mythscreentype.h"

class MythUITextEdit;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;
class MythUISpinBox;

class CustomPriority : public MythScreenType
{
    Q_OBJECT
  public:
    explicit CustomPriority(MythScreenStack *parent,
                            ProgramInfo *proginfo = NULL);
   ~CustomPriority();

    bool Create();

  protected slots:
    void ruleChanged(MythUIButtonListItem *item);

    void textChanged();

    void addClicked(void);
    void testClicked(void);
    void installClicked(void);
    void deleteClicked(void);

  private:
    void loadData(void);
    void loadExampleRules(void);
    bool checkSyntax(void);
    void testSchedule(void);

    ProgramInfo *m_pginfo;

    MythUIButtonList *m_ruleList;
    MythUIButtonList *m_clauseList;

    MythUITextEdit *m_titleEdit;
    MythUITextEdit *m_descriptionEdit;

    MythUISpinBox *m_prioritySpin;

    MythUIButton *m_addButton;
    MythUIButton *m_testButton;
    MythUIButton *m_installButton;
    MythUIButton *m_deleteButton;
    MythUIButton *m_cancelButton;
};

struct RuleInfo {
    QString title;
    QString priority;
    QString description;
};

Q_DECLARE_METATYPE(RuleInfo)

#endif
