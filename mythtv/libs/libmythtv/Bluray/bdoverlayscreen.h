#ifndef BDOVERLAYSCREEN_H
#define BDOVERLAYSCREEN_H

#include "mythscreentype.h"
#include "mythplayer.h"

class BDOverlay;

class BDOverlayScreen : public MythScreenType
{
  public:
    BDOverlayScreen(MythPlayer *player, const QString &name);
   ~BDOverlayScreen() override;

    void DisplayBDOverlay(BDOverlay *overlay);

  private:
    MythPlayer *m_player {nullptr};
};

#endif // BDOVERLAYSCREEN_H
