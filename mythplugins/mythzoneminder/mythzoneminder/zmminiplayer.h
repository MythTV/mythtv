#ifndef ZMMINIPLAYER_H_
#define ZMMINIPLAYER_H_

#include <mythexp.h>
#include <zmliveplayer.h>

class QTimer;
class MythUIImage;

class MPUBLIC ZMMiniPlayer : public ZMLivePlayer
{
  Q_OBJECT

  public:
    ZMMiniPlayer(MythScreenStack *parent);
    ~ZMMiniPlayer();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent (QEvent* event);

    void setAlarmMonitor(int monID) { m_alarmMonitor = monID; }

  public slots:
    void timerTimeout(void);

  private:
    QTimer *m_displayTimer;

    MythUIText  *m_monitorText;
    MythUIText  *m_statusText;
    MythUIImage *m_image;
};

#endif
