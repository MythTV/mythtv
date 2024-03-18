#ifndef PROGRAMRECPROIRITY_H_
#define PROGRAMRECPROIRITY_H_

// C++
#include <vector>

// MythTV headers
#include "libmythtv/recordinginfo.h"
#include "libmythui/mythscreentype.h"

// MythFrontend
#include "schedulecommon.h"

class QDateTime;

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;
class MythUIStateType;
class ProgramRecPriority;

class RecordingRule;

// overloaded version of RecordingInfo with additional recording priority
// values so we can keep everything together and don't
// have to hit the db mulitiple times
class ProgramRecPriorityInfo : public RecordingInfo
{
    friend class ProgramRecPriority;

  public:
    ProgramRecPriorityInfo() = default;
    ProgramRecPriorityInfo(const ProgramRecPriorityInfo &/*other*/) = default;
    ProgramRecPriorityInfo &operator=(const ProgramRecPriorityInfo &other)
        { if (this != &other) clone(other); return *this; }
    ProgramRecPriorityInfo &operator=(const RecordingInfo &other)
        { if (this != &other) clone(other); return *this; }
    ProgramRecPriorityInfo &operator=(const ProgramInfo &other)
        { if (this != &other) clone(other); return *this; }
    virtual void clone(const ProgramRecPriorityInfo &other,
                       bool ignore_non_serialized_data = false);
    void clone(const RecordingInfo &other,
               bool ignore_non_serialized_data = false) override; // RecordingInfo
    void clone(const ProgramInfo &other,
               bool ignore_non_serialized_data = false) override; // RecordingInfo

    void clear(void) override; // RecordingInfo

    void ToMap(InfoMap &progMap,
               bool showrerecord = false,
               uint star_range = 10,
               uint date_format = 0) const override; // ProgramInfo

    RecordingType m_recType     {kNotRecording};
    int           m_matchCount  {0};
    int           m_recCount    {0};
    QDateTime     m_last_record;
    int           m_avg_delay   {0};
    QString       m_profile;
    QString       m_recordingGroup;
    QString       m_storageGroup;
};

class ProgramRecPriority : public ScheduleCommon
{
    Q_OBJECT
  public:
    ProgramRecPriority(MythScreenStack *parent, const QString &name);
   ~ProgramRecPriority() override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // ScheduleCommon

    enum SortType : std::uint8_t
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
    void edit(MythUIButtonListItem *item) const;
    void scheduleChanged(int recid);

  private:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType

    void FillList(void);
    void SortList(ProgramRecPriorityInfo *newCurrentItem = nullptr);
    void UpdateList();
    void RemoveItemFromList(MythUIButtonListItem *item);

    void changeRecPriority(int howMuch);
    void saveRecPriority(void);
    void newTemplate(QString category);
    void remove();
    void deactivate();

    void showMenu(void);
    void showSortMenu(void);

    ProgramInfo *GetCurrentProgram(void) const override; // ScheduleCommon

    QMap<int, ProgramRecPriorityInfo> m_programData;
    std::vector<ProgramRecPriorityInfo*> m_sortedProgram;
    QMap<int, int> m_origRecPriorityData;

    void countMatches(void);
    QMap<int, int> m_conMatch;
    QMap<int, int> m_nowMatch;
    QMap<int, int> m_recMatch;
    QMap<int, int> m_listMatch;

    MythUIButtonList *m_programList       {nullptr};

    MythUIText *m_schedInfoText           {nullptr};
    MythUIText *m_recPriorityText         {nullptr};
    MythUIText *m_recPriorityBText        {nullptr};
    MythUIText *m_finalPriorityText       {nullptr};
    MythUIText *m_lastRecordedText        {nullptr};
    MythUIText *m_lastRecordedDateText    {nullptr};
    MythUIText *m_lastRecordedTimeText    {nullptr};
    MythUIText *m_chanNameText            {nullptr};
    MythUIText *m_chanNumText             {nullptr};
    MythUIText *m_callSignText            {nullptr};
    MythUIText *m_recProfileText          {nullptr};

    ProgramRecPriorityInfo *m_currentItem {nullptr};

    bool     m_reverseSort                {false};

    SortType m_sortType                   {byTitle};
};

Q_DECLARE_METATYPE(ProgramRecPriorityInfo *)

#endif
