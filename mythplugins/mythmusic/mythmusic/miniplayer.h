#ifndef MINIPLAYER_H_
#define MINIPLAYER_H_

#include <mythscreentype.h>

#include "musiccommon.h"

class QTimer;

class MPUBLIC MiniPlayer : public MusicCommon
{
  Q_OBJECT

  public:
    MiniPlayer(MythScreenStack *parent);
    ~MiniPlayer();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  public slots:
    void timerTimeout(void);

  private:
    QTimer       *m_displayTimer;
};

#endif
