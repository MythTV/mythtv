#ifndef PROGRAMRECPROIRITY_H_
#define PROGRAMRECPROIRITY_H_

#include "programinfo.h"
#include "scheduledrecording.h"
#include "mythscreentype.h"

class QDateTime;

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;
class MythUIStateType;

class ProgramRecPriorityInfo : public ProgramInfo
{
  public:
    ProgramRecPriorityInfo();
    ProgramRecPriorityInfo(const ProgramRecPriorityInfo &other);
    ProgramRecPriorityInfo& operator=(const ProgramInfo&);


    int recTypeRecPriority;
    RecordingType recType;
    int matchCount;
    int recCount;
    QDateTime last_record;
    int avg_delay;
    int autoRecPriority;
};

class ProgramRecPriority : public MythScreenType
{
    Q_OBJECT
  public:
    ProgramRecPriority(MythScreenStack *parent);
   ~ProgramRecPriority();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *event);

    enum SortType
    {
        byTitle,
        byRecPriority,
        byRecType,
        byCount,
        byRecCount,
        byLastRecord,
        byAvgDelay
    };

  protected slots:
    void updateInfo(MythUIButtonListItem *item);
    void edit(MythUIButtonListItem *item);
    void doRemove(bool doRemove);

  private:
    void FillList(void);
    void SortList(void);
    void UpdateList();
    void RemoveItemFromList(MythUIButtonListItem *item);

    void changeRecPriority(int howMuch);
    void saveRecPriority(void);
    void customEdit();
    void remove();
    void deactivate();
    void upcoming();
    void details();

    void showMenu(void);
    void showSortMenu(void);

    QMap<QString, ProgramRecPriorityInfo> m_programData;
    QMap<QString, ProgramRecPriorityInfo*> m_sortedProgram;
    QMap<int, int> m_origRecPriorityData;

    void countMatches(void);
    QMap<int, int> m_conMatch;
    QMap<int, int> m_nowMatch;
    QMap<int, int> m_recMatch;
    QMap<int, int> m_listMatch;

    MythUIButtonList *m_programList;

    MythUIText *m_schedInfoText;
    MythUIText *m_rectypePriorityText;
    MythUIText *m_recPriorityText;
    MythUIText *m_recPriorityBText;
    MythUIText *m_finalPriorityText;

    ProgramRecPriorityInfo *m_currentItem;

    bool m_reverseSort;

    SortType m_sortType;
};

Q_DECLARE_METATYPE(ProgramRecPriorityInfo *)

#endif
