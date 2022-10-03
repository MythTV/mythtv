#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

// QT
#include <QDateTime>
#include <QString>
#include <QMap>

// MythTV
#include "libmythbase/programinfo.h"
#include "libmythui/mythscreentype.h"

// MythFrontend
#include "schedulecommon.h"

class TV;
class Timer;

class MythUIText;
class MythUIStateType;
class MythUIButtonList;
class MythUIButtonListItem;

/**
 * \class ViewScheduled
 *
 * \brief Screen for viewing and managing upcoming and conflicted recordings
 */
class ViewScheduled : public ScheduleCommon
{
    Q_OBJECT
  public:
    explicit ViewScheduled(MythScreenStack *parent, TV *player = nullptr,
                  bool showTV = false);
    ~ViewScheduled() override;

    static void * RunViewScheduled(void *player, bool showTv);

    bool Create(void) override; // MythScreenType
    void ShowMenu(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // ScheduleCommon

  protected slots:
    void ChangeGroup(MythUIButtonListItem *item);
    void deleteRule();
    void updateInfo(MythUIButtonListItem *item);
    void SwitchList(void);
    void Close(void) override; // MythScreenType

  protected:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType
    ProgramInfo *GetCurrentProgram(void) const override; // ScheduleCommon

  private:
    void FillList(void);
    void LoadList(bool useExistingData = false);
    void setShowAll(bool all);
    void viewInputs(void);

    void EmbedTVWindow(void);

    void UpdateUIListItem(MythUIButtonListItem* item,
                          ProgramInfo *pginfo) const;
    static void CalcRecordedPercent(ProgramInfo &pg);

    bool  m_conflictBool              {false};
    QDate m_conflictDate;

    QRect m_tvRect;

    MythUIButtonList *m_schedulesList {nullptr};
    MythUIButtonList *m_groupList     {nullptr};
    MythUIProgressBar *m_progressBar  {nullptr};

    bool              m_showAll       {false};

    bool              m_inEvent       {false};
    bool              m_inFill        {false};
    bool              m_needFill      {false};

    int               m_listPos       {0};
    ProgramList m_recList;
    QMap<QDate, ProgramList> m_recgroupList;

    QDate m_currentGroup;
    QDate m_defaultGroup;

    QMap<int, int> m_inputref;
    uint           m_maxinput         {0};
    uint           m_curinput         {0};

    TV            *m_player           {nullptr};
};

#endif
