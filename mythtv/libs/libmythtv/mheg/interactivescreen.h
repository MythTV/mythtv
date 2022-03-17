#ifndef INTERACTIVESCREEN_H
#define INTERACTIVESCREEN_H

// MythTV
#include "libmythui/mythscreentype.h"

class MythPlayerUI;

class InteractiveScreen : public MythScreenType
{
    Q_OBJECT
  public:
    InteractiveScreen(MythPlayerUI* Player, MythPainter* Painter, const QString& Name);
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
    MythPlayerUI* m_player { nullptr };
    QRect         m_safeArea;
};

#endif
