#ifndef MYTHPLAYERUIBASE_H
#define MYTHPLAYERUIBASE_H

// MythTV
#include "mythplayer.h"

class MTV_PUBLIC MythPlayerUIBase : public MythPlayer
{
    Q_OBJECT

  public:
    MythPlayerUIBase(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);

    MythRender* GetRender() const;

  protected:
    MythMainWindow* m_mainWindow { nullptr };
    TV*             m_tv         { nullptr };
    MythRender*     m_render     { nullptr };
    MythPainter*    m_painter    { nullptr };
    MythDisplay*    m_display    { nullptr };

  protected slots:
    virtual void InitialiseState();
};

#endif
