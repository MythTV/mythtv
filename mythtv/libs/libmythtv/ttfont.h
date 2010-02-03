#ifndef INCLUDED_TTFONT__H_
#define INCLUDED_TTFONT__H_

#include "mythconfig.h"

#if HAVE_STDINT_H
#include <stdint.h>
#endif

struct Raster_Map;
class OSDSurface;

enum kTTF_Color {
    kTTF_Normal = 0, 
    kTTF_Outline, 
    kTTF_Shadow,
};

class TTFFontPrivate;
class TTFFont
{
  public:
     TTFFont(const QString &file, int size, float wscale, float hmult);
     TTFFont(const TTFFont&);
    ~TTFFont();

     // Actually greyscale, keep for compat.
     void setColor(int color);
     void setColor(const QColor &c, kTTF_Color k = kTTF_Normal);

     void setOutline(bool outline) { m_outline = outline; }
     void setShadow(int xoff, int yoff) { m_shadowxoff = xoff; 
                                          m_shadowyoff = yoff; }

     void DrawString(OSDSurface *surface, int x, int y, const QString &text,
                     int maxx, int maxy, int alphamod = 255,
                     bool double_size = false) const; 
     void CalcWidth(const QString &text, int *width_return) const;

     bool isValid(void)    const;
     int  SpaceWidth(void) const;
     int  Size(void)       const;

     bool Reinit(float wscale, float hmult);

  private:
     void MergeText(OSDSurface *surface, Raster_Map *rmap, int offset_x, 
                    int offset_y, int xstart, int ystart, int width, 
                    int height, int alphamod, kTTF_Color k = kTTF_Normal) const;

     TTFFontPrivate *m_priv;

     bool m_outline;
     int m_shadowxoff;
     int m_shadowyoff;

     uint8_t m_color_normal_y;
     uint8_t m_color_normal_u;
     uint8_t m_color_normal_v;

     uint8_t m_color_outline_y;
     uint8_t m_color_outline_u;
     uint8_t m_color_outline_v;

     uint8_t m_color_shadow_y;
     uint8_t m_color_shadow_u;
     uint8_t m_color_shadow_v;
};

#endif
