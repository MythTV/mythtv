#ifndef MINIPLAYER_H_
#define MINIPLAYER_H_

// MythTV
#include <libmythbase/mythpluginexport.h>
#include <libmythui/mythscreentype.h>

#include "musiccommon.h"

class QTimer;

class MPLUGIN_PUBLIC MiniPlayer : public MusicCommon
{
  Q_OBJECT

  public:
    explicit MiniPlayer(MythScreenStack *parent);
    ~MiniPlayer() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MusicCommon

  public slots:
    void timerTimeout(void);

  private:
    QTimer *m_displayTimer {nullptr};
};

#endif
