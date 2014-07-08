#ifndef _IDLESCREEN_H_
#define _IDLESCREEN_H_

#include <mythscreentype.h>
// libmyth
#include "programinfo.h"

class MythUIStateType;
class MythUIButtonList;
class QTimer;

class IdleScreen : public MythScreenType
{
    Q_OBJECT

  public:
    IdleScreen(MythScreenStack *parent);
    virtual ~IdleScreen();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *e);


  public slots:
    void UpdateStatus(void);
    void UpdateScreen(void);
    bool UpdateScheduledList();

  protected:
    void Load(void);
    void Init(void);

  private:
    bool CheckConnectionToServer(void);
    bool PendingSchedUpdate() const             { return m_pendingSchedUpdate; }
    void SetPendingSchedUpdate(bool newState)   { m_pendingSchedUpdate = newState; }

    QTimer        *m_updateScreenTimer;

    MythUIStateType  *m_statusState;
    MythUIButtonList *m_currentRecordings;
    MythUIButtonList *m_nextRecordings;
    MythUIButtonList *m_conflictingRecordings;
    MythUIText       *m_conflictWarning;

    int             m_secondsToShutdown;

    QMutex          m_schedUpdateMutex;
    bool            m_pendingSchedUpdate;
    ProgramList     m_scheduledList;
    bool            m_hasConflicts;
};

#endif
