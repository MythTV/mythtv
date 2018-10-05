#ifndef VIEWSCHEDULEDIFF_H_
#define VIEWSCHEDULEDIFF_H_

// C++ headers
#include <vector>

// mythui
#include "mythscreentype.h"

// mythtv
#include "programinfo.h"

class ProgramStruct
{
  public:
    ProgramStruct() : before(nullptr), after(nullptr) {}
    ProgramInfo *before;
    ProgramInfo *after;
};

class QKeyEvent;

class ViewScheduleDiff : public MythScreenType
{
    Q_OBJECT
  public:
    ViewScheduleDiff(MythScreenStack *parent, QString altTbl,
                     int recordid = -1, QString ltitle = "");
    ~ViewScheduleDiff() = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

  private slots:
   void updateInfo(MythUIButtonListItem *item);
   void showStatus(MythUIButtonListItem *item);

  private:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType

    void fillList(void);
    void updateUIList();

    ProgramInfo *CurrentProgram(void);

    bool m_inEvent;
    bool m_inFill;

    ProgramList m_recListBefore;
    ProgramList m_recListAfter;

    QString m_altTable;
    QString m_title;

    MythUIButtonList *m_conflictList;
    MythUIText       *m_titleText;
    MythUIText       *m_noChangesText;

    std::vector<class ProgramStruct> m_recList;

    int m_recordid; ///< recordid that differs from master (-1 = assume all)
};

#endif
