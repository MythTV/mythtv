#ifndef BDOVERLAYSCREEN_H
#define BDOVERLAYSCREEN_H

// MythTV
#include "mythscreentype.h"
#include "mythplayer.h"

class MythBDOverlay;

class MythBDOverlayScreen : public MythScreenType
{
  public:
    MythBDOverlayScreen(MythPlayer *Player, const QString &Name);
   ~MythBDOverlayScreen() override;

    void DisplayBDOverlay(MythBDOverlay *Overlay);

  private:
    MythPlayer *m_player { nullptr };
};

#endif // BDOVERLAYSCREEN_H
