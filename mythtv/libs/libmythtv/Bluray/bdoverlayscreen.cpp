#include <QPainter>

#include "mythuiimage.h"
#include "mythpainter.h"

#include "bdringbuffer.h"
#include "bdoverlayscreen.h"

#define LOC QString("BDScreen: ")

BDOverlayScreen::BDOverlayScreen(MythPlayer *player, const QString &name)
  : MythScreenType((MythScreenType*)nullptr, name),
    m_player(player)
{
}

BDOverlayScreen::~BDOverlayScreen()
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "dtor");
}

void BDOverlayScreen::DisplayBDOverlay(BDOverlay *overlay)
{
    if (!overlay || !m_player)
        return;

    MythRect rect(overlay->m_x, overlay->m_y,
                  overlay->m_image.width(), overlay->m_image.height());
    SetArea(rect);
    DeleteAllChildren();

    MythVideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;

    QImage& img = overlay->m_image;

    // add to screen
    QRect scaled = vo->GetImageRect(rect);
    if (scaled.size() != rect.size())
    {
        img = img.scaled(scaled.width(), scaled.height(),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    MythPainter *osd_painter = vo->GetOSDPainter();
    MythImage* image = nullptr;
    if (osd_painter)
         image = osd_painter->GetFormatImage();

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

    delete overlay;
}
