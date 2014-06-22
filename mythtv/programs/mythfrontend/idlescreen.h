#ifndef _IDLESCREEN_H_
#define _IDLESCREEN_H_

#include <mythscreentype.h>
// libmyth
#include "programinfo.h"

class MythUIStateType;
class MythUIText;
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
    bool UpdateScheduledList(void);

  protected:
    void Load(void);
    void Init(void);

  private:
    bool CheckConnectionToServer(void);
    bool PendingSchedUpdate() const           { return m_pendingSchedUpdate; }
    void SetPendingSchedUpdate(bool newState) { m_pendingSchedUpdate = newState; }

    QTimer        *m_updateStatusTimer;

    MythUIStateType  *m_statusState;
    MythUIText       *m_scheduledText;
    MythUIText       *m_warningText;

    int m_secondsToShutdown;
    bool m_backendRecording;

    QMutex          m_schedUpdateMutex;
    bool            m_pendingSchedUpdate;
    bool            m_hasConflicts;
    QDateTime       m_nextRecordingStart;
    vector<ProgramInfo> m_scheduledList;
};

#endif
