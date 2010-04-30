#ifndef VIEWSCHEDULEDIFF_H_
#define VIEWSCHEDULEDIFF_H_

// C++ headers
#include <vector>
using namespace std;

// mythui
#include "mythscreentype.h"

// mythtv
#include "programinfo.h"
#include "programlist.h"

class ProgramStruct
{
  public:
    ProgramStruct() : before(NULL), after(NULL) {}
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
    ~ViewScheduleDiff();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private slots:
   void updateInfo(MythUIButtonListItem *item);
   void showStatus(MythUIButtonListItem *item);

  private:
    virtual void Load(void);
    virtual void Init(void);

    void fillList(void);
    void updateUIList();

    ProgramInfo *CurrentProgram(void);

    QString m_dateformat;
    QString m_timeformat;
    QString m_channelFormat;

    bool m_inEvent;
    bool m_inFill;

    ProgramList m_recListBefore;
    ProgramList m_recListAfter;

    QString m_altTable;
    QString m_title;

    MythUIButtonList *m_conflictList;
    MythUIText       *m_titleText;
    MythUIText       *m_noChangesText;

    vector<class ProgramStruct> m_recList;

    int m_recordid; ///< recordid that differs from master (-1 = assume all)

    int m_programCnt;
};

#endif
