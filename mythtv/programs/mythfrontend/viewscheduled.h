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
    explicit ViewScheduled(MythScreenStack *parent, TV *player = NULL,
                  bool showTV = false);
    ~ViewScheduled();

    static void * RunViewScheduled(void *player, bool);

    virtual bool Create(void); // MythScreenType
    virtual void ShowMenu(void); // MythScreenType
    virtual bool keyPressEvent(QKeyEvent *); // QObject
    virtual void customEvent(QEvent*); // QObject

  protected slots:
    void ChangeGroup(MythUIButtonListItem *item);
    void showGuide();
    void edit();
    void customEdit();
    void deleteRule();
    void upcoming();
    void upcomingScheduled();
    void details();
    void selected(MythUIButtonListItem *);
    void updateInfo(MythUIButtonListItem *);
    void SwitchList(void);
    void Close(void);

  protected:
    virtual void Load(void); // MythScreenType
    virtual void Init(void); // MythScreenType

  private:
    void FillList(void);
    void LoadList(bool useExistingData = false);
    void setShowAll(bool all);
    void viewCards(void);
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

    QMap<int, int> m_cardref;
    uint m_maxcard;
    uint m_curcard;

    QMap<int, int> m_inputref;
    uint m_maxinput;
    uint m_curinput;

    TV *m_player;
};

#endif
