#ifndef INCLUDED_TTFONT__H_
#define INCLUDED_TTFONT__H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <qstring.h>
#include <qmap.h>
#include <qcolor.h>

#include "config.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

struct Raster_Map;
class OSDSurface;

enum kTTF_Color {
    kTTF_Normal = 0, 
    kTTF_Outline, 
    kTTF_Shadow,
};

class TTFFont
{
  public:
     TTFFont(char *file, int size, float wscale,
             float hmult);
    ~TTFFont();

     // Actually greyscale, keep for compat.
     void setColor(int color);
     void setColor(QColor c, kTTF_Color k = kTTF_Normal);

     void setOutline(bool outline) { m_outline = outline; }
     void setShadow(int xoff, int yoff) { m_shadowxoff = xoff; 
                                          m_shadowyoff = yoff; }

     bool isValid(void) { return valid; }

     void DrawString(OSDSurface *surface, int x, int y, const QString &text,
                     int maxx, int maxy, int alphamod = 255,
                     bool double_size = false); 
     void CalcWidth(const QString &text, int *width_return);

     int SpaceWidth() { return spacewidth; }
     int Size() { return loadedfontsize; }

     void Reinit(float wscale, float hmult);

  private:
     void KillFace(void);
     void Init(void);

     Raster_Map *create_font_raster(int width, int height);
     Raster_Map *duplicate_raster(FT_BitmapGlyph bmap);
     void clear_raster(Raster_Map *rmap);
     void destroy_font_raster(Raster_Map *rmap);
     Raster_Map *calc_size(int *width, int *height, const QString &text,
                           bool double_size = false);
     void render_text(Raster_Map *rmap, Raster_Map *rchr, const QString &text, 
                      int *xorblah, int *yor, bool double_size = false);
     void merge_text(OSDSurface *surface, Raster_Map *rmap, int offset_x, 
                     int offset_y, int xstart, int ystart, int width, 
                     int height, int alphamod, kTTF_Color k = kTTF_Normal);
     bool cache_glyph(unsigned short c);

     bool         valid;
     FT_Library   library;
     FT_Face      face;
     QMap<unsigned short, FT_Glyph> glyphs;
     QMap<unsigned short, Raster_Map *> glyphs_cached;
     int          max_descent;
     int          max_ascent;
     int          fontsize;
     int          vid_width;
     int          vid_height;
     bool         use_kerning;

     int spacewidth;
     int m_size;

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

     QString m_file;

     int loadedfontsize;
     float m_wscale;
     float m_hmult;
};

#endif
