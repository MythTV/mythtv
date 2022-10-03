#ifndef CUSTOMPRIORITY_H_
#define CUSTOMPRIORITY_H_

#include "libmythbase/programinfo.h"
#include "libmythui/mythscreentype.h"

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
                            ProgramInfo *proginfo = nullptr);
   ~CustomPriority() override;

    bool Create() override; // MythScreenType

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

    ProgramInfo *m_pginfo             {nullptr};

    MythUIButtonList *m_ruleList      {nullptr};
    MythUIButtonList *m_clauseList    {nullptr};

    MythUITextEdit *m_titleEdit       {nullptr};
    MythUITextEdit *m_descriptionEdit {nullptr};

    MythUISpinBox *m_prioritySpin     {nullptr};

    MythUIButton *m_addButton         {nullptr};
    MythUIButton *m_testButton        {nullptr};
    MythUIButton *m_installButton     {nullptr};
    MythUIButton *m_deleteButton      {nullptr};
    MythUIButton *m_cancelButton      {nullptr};
};

struct RuleInfo {
    QString title;
    QString priority;
    QString description;
};

Q_DECLARE_METATYPE(RuleInfo)

#endif
