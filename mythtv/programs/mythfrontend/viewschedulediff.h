#ifndef VIEWSCHEDULEDIFF_H_
#define VIEWSCHEDULEDIFF_H_

// C++ headers
#include <utility>
#include <vector>

// MythTV
#include "libmythbase/programinfo.h"
#include "libmythui/mythscreentype.h"

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
    ViewScheduleDiff(MythScreenStack *parent, QString  altTbl,
                     int recordid = -1, QString  ltitle = "")
        : MythScreenType(parent, "ViewScheduleDiff"),
          m_altTable(std::move(altTbl)), m_title(std::move(ltitle)),
          m_recordid(recordid) {}
    ~ViewScheduleDiff() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

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
