#ifndef MYTHBDOVERLAY_H
#define MYTHBDOVERLAY_H

// Qt
#include <QImage>

// BluRay
#ifdef HAVE_LIBBLURAY
#include <libbluray/bluray.h>
#include <libbluray/overlay.h>
#else
#include "libbluray/bluray.h"
#include "libbluray/decoders/overlay.h"
#endif

class MythBDOverlay
{
  public:
    MythBDOverlay() = default;
    explicit MythBDOverlay(const bd_overlay_s* Overlay);
    explicit MythBDOverlay(const bd_argb_overlay_s* Overlay);

    void     SetPalette(const BD_PG_PALETTE_ENTRY* Palette);
    void     Wipe(void);
    void     Wipe(int Left, int Top, int Width, int Height);

    QImage   m_image;
    int64_t  m_pts   { -1 };
    int      m_x     { 0  };
    int      m_y     { 0  };
};
#endif // MYTHBDOVERLAY_H
