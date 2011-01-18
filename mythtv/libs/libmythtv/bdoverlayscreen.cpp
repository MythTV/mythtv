#include <QPainter>

#include "mythuiimage.h"
#include "mythpainter.h"

#include "bdringbuffer.h"
#include "bdoverlayscreen.h"

#define LOC QString("BDScreen: ")
#define ERR QString("BDScreen Error: ")

BDOverlayScreen::BDOverlayScreen(MythPlayer *player, const QString &name)
  : MythScreenType((MythScreenType*)NULL, name),
    m_player(player), m_overlayArea(QRect())
{
}

BDOverlayScreen::~BDOverlayScreen()
{
    VERBOSE(VB_PLAYBACK, LOC + "dtor");
    m_overlayMap.clear();
}

void BDOverlayScreen::DisplayBDOverlay(BDOverlay *overlay)
{
    if (!overlay || !m_player)
        return;

    if (!overlay->m_data)
    {
        m_overlayArea = overlay->m_position;
        SetArea(MythRect(m_overlayArea));
        DeleteAllChildren();
        m_overlayMap.clear();
        SetRedraw();
        VERBOSE(VB_PLAYBACK, LOC +
            QString("Initialised Size: %1x%2 (%3+%4) Plane: %5 Pts: %6")
                .arg(overlay->m_position.width())
                .arg(overlay->m_position.height())
                .arg(overlay->m_position.left())
                .arg(overlay->m_position.top())
                .arg(overlay->m_plane)
                .arg(overlay->m_pts));
        BDOverlay::DeleteOverlay(overlay);
        return;
    }

    if (!m_overlayArea.isValid())
    {
        VERBOSE(VB_IMPORTANT, ERR + "Error: Overlay image submitted "
                                    "before initialisation.");
    }

    VideoOutput *vo = m_player->getVideoOutput();
    if (!vo)
        return;

    QRect   rect = overlay->m_position;
    QString hash = QString("%1+%2+%3x%4")
                    .arg(rect.left()).arg(rect.top())
                    .arg(rect.width()).arg(rect.height());

    // remove if we already have this overlay
    if (m_overlayMap.contains(hash))
    {
        VERBOSE(VB_PLAYBACK|VB_EXTRA, LOC + QString("Removing %1 (%2 left)")
            .arg(hash).arg(m_overlayMap.size()));
        MythUIImage *old = m_overlayMap.take(hash);
        DeleteChild(old);
    }

    // convert the overlay palette to ARGB
    uint32_t *origpalette = (uint32_t *)(overlay->m_palette);
    QVector<unsigned int> palette;
    for (int i = 0; i < 256; i++)
    {
        int y  = (origpalette[i] >> 0) & 0xff;
        int cr = (origpalette[i] >> 8) & 0xff;
        int cb = (origpalette[i] >> 16) & 0xff;
        int a  = (origpalette[i] >> 24) & 0xff;
        int r  = int(y + 1.4022 * (cr - 128));
        int b  = int(y + 1.7710 * (cb - 128));
        int g  = int(1.7047 * y - (0.1952 * b) - (0.5647 * r));
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
        if (r > 0xff) r = 0xff;
        if (g > 0xff) g = 0xff;
        if (b > 0xff) b = 0xff;
        palette.push_back((a << 24) | (r << 16) | (g << 8) | b);
    }

    // convert the image to QImage
    QImage img(rect.size(), QImage::Format_Indexed8);
    memcpy(img.bits(), overlay->m_data, rect.width() * rect.height());
    img.setColorTable(palette);
    img.convertToFormat(QImage::Format_ARGB32);

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
            m_overlayMap.insert(hash, uiimage);
            VERBOSE(VB_PLAYBACK|VB_EXTRA, LOC + QString("Added %1 (%2 tot)")
                .arg(hash).arg(m_overlayMap.size()));
        }
    }

    SetRedraw();
    BDOverlay::DeleteOverlay(overlay);
}
