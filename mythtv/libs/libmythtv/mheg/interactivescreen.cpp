// MythTV
#include "mythplayerui.h"
#include "mheg/interactivescreen.h"

InteractiveScreen::InteractiveScreen(MythPlayerUI *Player, MythPainter *Painter, const QString &Name)
  : MythScreenType(static_cast<MythScreenType*>(nullptr), Name),
    m_player(Player)
{
    m_painter = Painter;
    UpdateArea();
}

void InteractiveScreen::Close()
{
    if (m_player)
        emit m_player->ResizeForInteractiveTV(QRect());
}

void InteractiveScreen::UpdateArea()
{
    if (m_childrenList.isEmpty())
    {
        m_safeArea = {};
    }
    else if (m_player && m_player->GetVideoOutput())
    {
        float tmp = 0.0;
        QRect dummy;
        m_player->GetVideoOutput()->GetOSDBounds(dummy, m_safeArea, tmp, tmp, tmp);
    }
    SetArea(MythRect(m_safeArea));
}

void InteractiveScreen::OptimiseDisplayedArea()
{
    UpdateArea();

    QRegion visible;
    for (const auto *img : std::as_const(m_childrenList))
        visible = visible.united(img->GetArea());

    if (visible.isEmpty())
        return;

    QRect bounding  = visible.boundingRect();
    bounding = bounding.translated(m_safeArea.topLeft());
    bounding = m_safeArea.intersected(bounding);
    int left = m_safeArea.left() - bounding.left();
    int top  = m_safeArea.top()  - bounding.top();
    SetArea(MythRect(bounding));

    for (auto *img : std::as_const(m_childrenList))
        img->SetArea(MythRect(img->GetArea().translated(left, top)));
}

#include "moc_interactivescreen.cpp"
