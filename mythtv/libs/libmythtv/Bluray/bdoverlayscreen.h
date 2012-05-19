#ifndef BDOVERLAYSCREEN_H
#define BDOVERLAYSCREEN_H

#include "mythscreentype.h"
#include "mythplayer.h"

class BDOverlay;

class BDOverlayScreen : public MythScreenType
{
  public:
    BDOverlayScreen(MythPlayer *player, const QString &name);
   ~BDOverlayScreen();

    void DisplayBDOverlay(BDOverlay *overlay);

  private:
    MythPlayer *m_player;
    QRect       m_overlayArea;
    QMap<QString,MythUIImage*> m_overlayMap;
};

#endif // BDOVERLAYSCREEN_H
