#ifndef INTERACTIVESCREEN_H
#define INTERACTIVESCREEN_H

#include "mythscreentype.h"
#include "mythplayer.h"

class InteractiveScreen : public MythScreenType
{
  public:
    InteractiveScreen(MythPlayer *player, const QString &name);
    virtual ~InteractiveScreen() = default;
    bool Create(void) override // MythScreenType
    {
        SetArea(MythRect());
        return true;
    }
    void UpdateArea(void);
    void OptimiseDisplayedArea(void);

  public slots:
    void Close() override; // MythScreenType

  private:
    MythPlayer *m_player {nullptr};
    QRect       m_safeArea;
};

#endif // INTERACTIVESCREEN_H
