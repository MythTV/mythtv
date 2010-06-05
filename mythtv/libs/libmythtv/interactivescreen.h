#ifndef INTERACTIVESCREEN_H
#define INTERACTIVESCREEN_H

#include "mythscreentype.h"
#include "NuppelVideoPlayer.h"

class InteractiveScreen : public MythScreenType
{
  public:
    InteractiveScreen(NuppelVideoPlayer *player, const QString &name);
    virtual ~InteractiveScreen();
    virtual bool Create(void)
    {
        SetArea(MythRect());
        return true;
    }
    void UpdateArea(void);
    void OptimiseDisplayedArea(void);

  public slots:
    virtual void Close();

  private:
    NuppelVideoPlayer *m_player;
    QRect m_safeArea;
};

#endif // INTERACTIVESCREEN_H
