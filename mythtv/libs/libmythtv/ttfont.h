#ifndef INCLUDED_TTFONT__H_
#define INCLUDED_TTFONT__H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <qstring.h>

struct Raster_Map;

class TTFFont
{
  public:
     TTFFont(char *file, int size, int video_width, int video_height);
    ~TTFFont();

     // 0-255
     void setColor(int color) { m_color = color; }
     void setOutline(bool outline) { m_outline = outline; }
     void setShadow(int xoff, int yoff) { m_shadowxoff = xoff; 
                                          m_shadowyoff = yoff; }

     bool isValid(void) { return valid; }

     void DrawString(unsigned char *yuvptr, int x, int y, const QString &text,
                     int maxx, int maxy, int alphamod = 255); 
     void CalcWidth(const QString &text, int *width_return);

     int SpaceWidth() { return spacewidth; }
     int Size() { return m_size; }

  private:
     Raster_Map *create_font_raster(int width, int height);
     Raster_Map *duplicate_raster(FT_BitmapGlyph bmap);
     void clear_raster(Raster_Map *rmap);
     void destroy_font_raster(Raster_Map *rmap);
     Raster_Map *calc_size(int *width, int *height, char *text);
     void render_text(Raster_Map *rmap, Raster_Map *rchr, char *text, 
                      int *xorblah, int *yor);
     void merge_text(unsigned char *yuv, Raster_Map *rmap, int offset_x, 
                     int offset_y, int xstart, int ystart, int width, 
                     int height, int video_width, int video_height, int color,
                     int alphamod);

     bool         valid;
     FT_Library   library;
     FT_Face      face;
     int          num_glyph;
     FT_Glyph    *glyphs;
     Raster_Map **glyphs_cached;
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
     int m_color;
};

#endif
