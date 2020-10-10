#ifndef MYTHPLAYERUIBASE_H
#define MYTHPLAYERUIBASE_H

// MythTV
#include "mythplayer.h"

class MythPlayerUIBase : public MythPlayer
{
    Q_OBJECT

  public:
    MythPlayerUIBase(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);

    MythMainWindow* m_mainWindow { nullptr };
    TV*             m_tv         { nullptr };
    MythRender*     m_render     { nullptr };
    MythPainter*    m_painter    { nullptr };
    MythDisplay*    m_display    { nullptr };
};

#endif
