#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

// QT
#include <QDateTime>
#include <QString>
#include <QMap>

// MythTV
#include "schedulecommon.h"
#include "mythscreentype.h"
#include "programinfo.h"

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
    ~ViewScheduled();

    static void * RunViewScheduled(void *player, bool);

    bool Create(void) override; // MythScreenType
    void ShowMenu(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType
    void customEvent(QEvent*) override; // ScheduleCommon

  protected slots:
    void ChangeGroup(MythUIButtonListItem *item);
    void deleteRule();
    void updateInfo(MythUIButtonListItem *);
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

    bool m_conflictBool;
    QDate m_conflictDate;

    QRect m_tvRect;

    MythUIButtonList *m_schedulesList;
    MythUIButtonList *m_groupList;

    bool m_showAll;

    bool m_inEvent;
    bool m_inFill;
    bool m_needFill;

    int m_listPos;
    ProgramList m_recList;
    QMap<QDate, ProgramList> m_recgroupList;

    QDate m_currentGroup;
    QDate m_defaultGroup;

    QMap<int, int> m_inputref;
    uint m_maxinput;
    uint m_curinput;

    TV *m_player;
};

#endif
