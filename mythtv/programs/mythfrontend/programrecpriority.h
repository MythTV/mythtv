#ifndef PROGRAMRECPROIRITY_H_
#define PROGRAMRECPROIRITY_H_

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

    int recTypeRecPriority;
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

    QString m_formatShortDate;
    QString m_formatLongDate;
    QString m_formatTime;

    MythUIText *m_categoryText;
    MythUIText *m_descriptionText;
    MythUIText *m_schedInfoText;
    MythUIText *m_rectypePriorityText;
    MythUIText *m_recPriorityText;
    MythUIText *m_recPriorityBText;
    MythUIText *m_finalPriorityText;
    MythUIText *m_recGroupText;
    MythUIText *m_storageGroupText;
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
