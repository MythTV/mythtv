#include <QPainter>

#include "mythuiimage.h"
#include "mythpainter.h"

#include "bdringbuffer.h"
#include "bdoverlayscreen.h"

#define LOC QString("BDScreen: ")

BDOverlayScreen::BDOverlayScreen(MythPlayer *player, const QString &name)
  : MythScreenType((MythScreenType*)NULL, name),
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

    MythRect rect(overlay->x, overlay->y, overlay->image.width(), overlay->image.height());
    SetArea(rect);
    DeleteAllChildren();

    VideoOutput *vo = m_player->GetVideoOutput();
    if (!vo)
        return;

    QImage& img = overlay->image;

    // add to screen
    QRect scaled = vo->GetImageRect(rect);
    if (scaled.size() != rect.size())
    {
        img = img.scaled(scaled.width(), scaled.height(),
                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    MythPainter *osd_painter = vo->GetOSDPainter();
    MythImage* image = NULL;
    if (osd_painter)
         image = osd_painter->GetFormatImage();

    if (image)
    {
        image->Assign(img);
        MythUIImage *uiimage = new MythUIImage(this, "bdoverlay");
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
