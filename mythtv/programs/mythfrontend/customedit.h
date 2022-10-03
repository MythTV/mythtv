#ifndef CUSTOMEDIT_H_
#define CUSTOMEDIT_H_

#include "libmythbase/programinfo.h"
#include "libmythtv/mythplayer.h"
#include "libmythui/mythscreentype.h"

class MythUITextEdit;
class MythUIButton;
class MythUIButtonList;
class MythUIButtonListItem;

/**
 * A screen to create a fully custom recording.  The recording will be
 * created as a power search.
 */
class CustomEdit : public MythScreenType
{
    Q_OBJECT
  public:

    explicit CustomEdit(MythScreenStack *parent, ProgramInfo *m_pginfo = nullptr);
   ~CustomEdit(void) override;

    bool Create() override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // MythUIType

  protected slots:
    void ruleChanged(MythUIButtonListItem *item);
    void textChanged(void);
    void clauseChanged(MythUIButtonListItem *item);
    void clauseClicked(MythUIButtonListItem *item);
    void testClicked(void);
    void recordClicked(void);
    void storeClicked(void);
    void scheduleCreated(int ruleID);

  private:
    void loadData(void);
    void loadClauses(void);
    bool checkSyntax(void);
    void storeRule(bool is_search, bool is_new);
    void deleteRule(void);
    QString evaluate(QString clause);

    ProgramInfo *m_pginfo               {nullptr};
    QString m_baseTitle;

    int m_maxex                         {0};
    bool m_evaluate                     {true};

    QString m_seSuffix;
    QString m_exSuffix;

    MythUIButtonList *m_ruleList        {nullptr};
    MythUIButtonList *m_clauseList      {nullptr};

    MythUITextEdit   *m_titleEdit       {nullptr};

    // Contains the SQL statement
    MythUITextEdit   *m_descriptionEdit {nullptr};

    // Contains the additional SQL tables
    MythUITextEdit   *m_subtitleEdit    {nullptr};

    MythUIText       *m_clauseText      {nullptr};
    MythUIButton     *m_testButton      {nullptr};
    MythUIButton     *m_recordButton    {nullptr};
    MythUIButton     *m_storeButton     {nullptr};
    MythUIButton     *m_cancelButton    {nullptr};

    const MythUIButtonListItem *m_currentRuleItem {nullptr};
};

struct CustomRuleInfo {
    QString recordid;
    QString title;
    QString subtitle;
    QString description;
};

Q_DECLARE_METATYPE(CustomRuleInfo)

#endif
