#include <algorithm>

// Qt
#include <QPainter>

// MythTV
#include "Bluray/mythbdoverlay.h"

MythBDOverlay::MythBDOverlay(const bd_overlay_s* const Overlay)
  : m_image(Overlay->w, Overlay->h, QImage::Format_Indexed8),
    m_x(Overlay->x),
    m_y(Overlay->y)
{
    Wipe();
}

MythBDOverlay::MythBDOverlay(const bd_argb_overlay_s* const Overlay)
  : m_image(Overlay->w, Overlay->h, QImage::Format_ARGB32),
    m_x(Overlay->x),
    m_y(Overlay->y)
{
}

void MythBDOverlay::SetPalette(const BD_PG_PALETTE_ENTRY *Palette)
{
    if (!Palette)
        return;

    QVector<QRgb> rgbpalette;
    for (int i = 0; i < 256; i++)
    {
        int y  = Palette[i].Y;
        int cr = Palette[i].Cr;
        int cb = Palette[i].Cb;
        int a  = Palette[i].T;
        int r  = std::clamp(int(y + (1.4022 * (cr - 128))), 0, 0xff);
        int b  = std::clamp(int(y + (1.7710 * (cb - 128))), 0, 0xff);
        int g  = std::clamp(int((1.7047 * y) - (0.1952 * b) - (0.5647 * r)), 0, 0xff);
        rgbpalette.push_back(static_cast<uint>((a << 24) | (r << 16) | (g << 8) | b));
    }
    m_image.setColorTable(rgbpalette);
}

void MythBDOverlay::Wipe(void)
{
    Wipe(0, 0, m_image.width(), m_image.height());
}

void MythBDOverlay::Wipe(int Left, int Top, int Width, int Height)
{
    if (m_image.format() == QImage::Format_Indexed8)
    {
        uint8_t *data = m_image.bits();
        int32_t offset = (Top * m_image.bytesPerLine()) + Left;
        for (int i = 0; i < Height; i++ )
        {
            memset(&data[offset], 0xff, static_cast<size_t>(Width));
            offset += m_image.bytesPerLine();
        }
    }
    else
    {
        QColor   transparent(0, 0, 0, 255);
        QPainter painter(&m_image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(Left, Top, Width, Height, transparent);
    }
}

