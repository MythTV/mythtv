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
    explicit IdleScreen(MythScreenStack *parent);
    virtual ~IdleScreen();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *e) override; // MythUIType


  public slots:
    void UpdateStatus(void);
    void UpdateScreen(void);
    bool UpdateScheduledList();

  protected:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType

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
