#ifndef _IDLESCREEN_H_
#define _IDLESCREEN_H_

#include <mythscreentype.h>

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

  protected:
    void Load(void);
    void Init(void);

  private:
    bool CheckConnectionToServer(void);

    QTimer        *m_updateStatusTimer;

    MythUIStateType  *m_statusState;

    int m_secondsToShutdown;
    bool m_backendRecording;
};

#endif
