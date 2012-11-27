#ifndef PROGRAMRECPROIRITY_H_
#define PROGRAMRECPROIRITY_H_

#include <vector>

#include "recordinginfo.h"
#include "mythscreentype.h"

// mythfrontend
#include "schedulecommon.h"

class QDateTime;

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;
class MythUIStateType;
class ProgramRecPriority;

class RecordingRule;

class ProgramRecPriorityInfo : public RecordingInfo
{
    friend class ProgramRecPriority;

  public:
    ProgramRecPriorityInfo();
    ProgramRecPriorityInfo(const ProgramRecPriorityInfo &other);
    ProgramRecPriorityInfo &operator=(const ProgramRecPriorityInfo&);
    ProgramRecPriorityInfo &operator=(const RecordingInfo&);
    ProgramRecPriorityInfo &operator=(const ProgramInfo&);

    virtual ProgramRecPriorityInfo &clone(const ProgramRecPriorityInfo &other);
    virtual ProgramRecPriorityInfo &clone(const ProgramInfo &other);
    virtual void clear(void);

    virtual void ToMap(QHash<QString, QString> &progMap,
                       bool showrerecord = false,
                       uint star_range = 10) const;

    RecordingType recType;
    int matchCount;
    int recCount;
    QDateTime last_record;
    int avg_delay;
    int autoRecPriority;
    QString profile;
};

class ProgramRecPriority : public ScheduleCommon
{
    Q_OBJECT
  public:
    ProgramRecPriority(MythScreenStack *parent, const QString &name);
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
    void scheduleChanged(int recid);

  private:
    virtual void Load(void);
    virtual void Init(void);

    void FillList(void);
    void SortList(ProgramRecPriorityInfo *newCurrentItem = NULL);
    void UpdateList();
    void RemoveItemFromList(MythUIButtonListItem *item);

    void changeRecPriority(int howMuch);
    void saveRecPriority(void);
    void customEdit();
    void newTemplate(QString category);
    void remove();
    void deactivate();
    void upcoming();
    void details();

    void showMenu(void);
    void showSortMenu(void);

    QMap<int, ProgramRecPriorityInfo> m_programData;
    vector<ProgramRecPriorityInfo*> m_sortedProgram;
    QMap<int, int> m_origRecPriorityData;

    void countMatches(void);
    QMap<int, int> m_conMatch;
    QMap<int, int> m_nowMatch;
    QMap<int, int> m_recMatch;
    QMap<int, int> m_listMatch;

    MythUIButtonList *m_programList;

    MythUIText *m_schedInfoText;
    MythUIText *m_recPriorityText;
    MythUIText *m_recPriorityBText;
    MythUIText *m_finalPriorityText;
    MythUIText *m_lastRecordedText;
    MythUIText *m_lastRecordedDateText;
    MythUIText *m_lastRecordedTimeText;
    MythUIText *m_channameText;
    MythUIText *m_channumText;
    MythUIText *m_callsignText;
    MythUIText *m_recProfileText;

    ProgramRecPriorityInfo *m_currentItem;

    bool m_reverseSort;

    SortType m_sortType;
};

Q_DECLARE_METATYPE(ProgramRecPriorityInfo *)

#endif
