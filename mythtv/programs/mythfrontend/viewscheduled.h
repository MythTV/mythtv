#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <QDateTime>

#include "programlist.h"

#include "mythscreentype.h"

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
class ViewScheduled : public MythScreenType
{
    Q_OBJECT
  public:
    ViewScheduled(MythScreenStack *parent, TV *player = NULL,
                  bool showTV = false);
    ~ViewScheduled();

    static void * RunViewScheduled(void *player, bool);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent*);

  protected slots:
    void edit();
    void customEdit();
    void deleteRule();
    void upcoming();
    void details();
    void selected(MythUIButtonListItem *);
    void updateInfo(MythUIButtonListItem *);

  private:
    void FillList(void);
    void setShowAll(bool all);
    void viewCards(void);
    void viewInputs(void);

    void EmbedTVWindow(void);

    void SetTextFromMap(MythUIType *parent, QMap<QString, QString> &infoMap);

    bool m_conflictBool;
    QDate m_conflictDate;
    QString m_dateformat;
    QString m_timeformat;
    QString m_channelFormat;

    QRect m_tvRect;

    MythUIButtonList *m_schedulesList;

    bool m_showAll;

    bool m_inEvent;
    bool m_inFill;
    bool m_needFill;

    int m_listPos;
    ProgramList m_recList;

    QMap<int, int> m_cardref;
    int m_maxcard;
    int m_curcard;

    QMap<int, int> m_inputref;
    int m_maxinput;
    int m_curinput;

    TV *m_player;
};

#endif
