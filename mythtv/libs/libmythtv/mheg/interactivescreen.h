#ifndef INTERACTIVESCREEN_H
#define INTERACTIVESCREEN_H

#include "mythscreentype.h"
#include "mythplayer.h"

class InteractiveScreen : public MythScreenType
{
    Q_OBJECT
  public:
    InteractiveScreen(MythPlayer* Player, MythPainter* Painter, const QString& Name);
    ~InteractiveScreen() override = default;
    bool Create() override
    {
        SetArea(MythRect());
        return true;
    }
    void UpdateArea();
    void OptimiseDisplayedArea();

  public slots:
    void Close() override;

  private:
    MythPlayer* m_player { nullptr };
    QRect       m_safeArea;
};

#endif
