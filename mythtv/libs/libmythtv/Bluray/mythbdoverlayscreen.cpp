// Qt
#include <QPainter>

// MythTV
#include "libmythui/mythpainter.h"
#include "libmythui/mythuiimage.h"

#include "Bluray/mythbdbuffer.h"
#include "Bluray/mythbdoverlayscreen.h"

#define LOC QString("BDScreen: ")

MythBDOverlayScreen::MythBDOverlayScreen(MythPlayerUI *Player, MythPainter *Painter, const QString &Name)
  : MythScreenType(static_cast<MythScreenType*>(nullptr), Name),
    m_player(Player)
{
    m_painter = Painter;
}

void MythBDOverlayScreen::DisplayBDOverlay(MythBDOverlay *Overlay)
{
    if (!Overlay || !m_player)
        return;

    MythRect rect(Overlay->m_x, Overlay->m_y, Overlay->m_image.width(), Overlay->m_image.height());
    SetArea(rect);
    DeleteAllChildren();

    MythVideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;

    QImage& img = Overlay->m_image;

    // add to screen
    QRect scaled = vo->GetImageRect(rect);
    if (scaled.size() != rect.size())
        img = img.scaled(scaled.width(), scaled.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    MythImage* image = m_painter->GetFormatImage();
    if (image)
    {
        image->Assign(img);
        auto *uiimage = new MythUIImage(this, "bdoverlay");
        if (uiimage)
        {
            uiimage->SetImage(image);
            uiimage->SetArea(MythRect(scaled));
        }
        image->DecrRef();
    }

    SetRedraw();
    delete Overlay;
}
