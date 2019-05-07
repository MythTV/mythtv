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
    ProgramStruct() = default;
    ProgramInfo *m_before {nullptr};
    ProgramInfo *m_after  {nullptr};
};

class QKeyEvent;

class ViewScheduleDiff : public MythScreenType
{
    Q_OBJECT
  public:
    ViewScheduleDiff(MythScreenStack *parent, const QString& altTbl,
                     int recordid = -1, const QString& ltitle = "")
        : MythScreenType(parent, "ViewScheduleDiff"),
          m_altTable(altTbl), m_title(ltitle),
          m_recordid(recordid) {}
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

    bool m_inEvent                    {false};
    bool m_inFill                     {false};

    ProgramList m_recListBefore;
    ProgramList m_recListAfter;

    QString m_altTable;
    QString m_title;

    MythUIButtonList *m_conflictList  {nullptr};
    MythUIText       *m_titleText     {nullptr};
    MythUIText       *m_noChangesText {nullptr};

    std::vector<class ProgramStruct> m_recList;

    int m_recordid {-1}; ///< recordid that differs from master (-1 = assume all)
};

#endif
