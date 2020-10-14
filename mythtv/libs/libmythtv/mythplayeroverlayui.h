#ifndef MYTHPLAYEROVERLAYUI_H
#define MYTHPLAYEROVERLAYUI_H

// MythTV
#include "mythplayeruibase.h"

class MythPlayerOverlayUI : public MythPlayerUIBase
{
    Q_OBJECT

  public:
    MythPlayerOverlayUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerOverlayUI() override = default;

  private:
    Q_DISABLE_COPY(MythPlayerOverlayUI)
};

#endif
