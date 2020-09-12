#ifndef INTERACTIVESCREEN_H
#define INTERACTIVESCREEN_H

#include "mythscreentype.h"
#include "mythplayer.h"

class InteractiveScreen : public MythScreenType
{
    Q_OBJECT
  public:
    InteractiveScreen(MythPlayer *player, const QString &name);
    ~InteractiveScreen() override = default;
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
