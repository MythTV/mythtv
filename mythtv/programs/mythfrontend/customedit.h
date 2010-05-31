#ifndef CUSTOMEDIT_H_
#define CUSTOMEDIT_H_

#include "NuppelVideoPlayer.h"
#include "programinfo.h"
#include "mythscreentype.h"

class MythUITextEdit;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

class CustomEdit : public MythScreenType
{
    Q_OBJECT
  public:

    explicit CustomEdit(MythScreenStack *parent, ProgramInfo *m_pginfo = NULL);
   ~CustomEdit(void);

   bool Create();
   void customEvent(QEvent *event);

  protected slots:
    void ruleChanged(MythUIButtonListItem *item);
    void textChanged(void);
    void clauseChanged(MythUIButtonListItem *item);
    void clauseClicked(MythUIButtonListItem *item);
    void testClicked(void);
    void recordClicked(void);
    void storeClicked(void);
    void scheduleCreated(int);

  private:
    void loadData(void);
    void loadClauses(void);
    bool checkSyntax(void);
    void storeRule(bool is_search, bool is_new);
    void deleteRule(void);

    ProgramInfo *m_pginfo;

    int m_prevItem;
    int m_maxex;

    QString m_seSuffix;
    QString m_exSuffix;

    MythUIButtonList *m_ruleList;
    MythUIButtonList *m_clauseList;

    MythUITextEdit *m_titleEdit;

    // Contains the SQL statement
    MythUITextEdit *m_descriptionEdit;

    // Contains the additional SQL tables
    MythUITextEdit *m_subtitleEdit;

    MythUIText   *m_clauseText;
    MythUIButton *m_testButton;
    MythUIButton *m_recordButton;
    MythUIButton *m_storeButton;
    MythUIButton *m_cancelButton;
};

struct CustomRuleInfo {
    QString recordid;
    QString title;
    QString subtitle;
    QString description;
};

Q_DECLARE_METATYPE(CustomRuleInfo)

#endif
