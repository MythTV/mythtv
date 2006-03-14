/*
 * Copyright (C) 1999 Carsten Haitzler and various contributors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <iostream>

using namespace std;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ttfont.h"

#include "osdtypes.h"
#include "osdsurface.h"

static int          have_library = 0;
static FT_Library   the_library;

#define FT_VALID(handle) ((handle) && (handle)->clazz != NULL)

struct Raster_Map
{
    int width;
    int rows;
    int cols;
    int size;
    unsigned char *bitmap;
};

void TTFFont::setColor(int color)
{
    color %= 256;
    m_color_normal_y = color;
    m_color_normal_u = m_color_normal_v = 128;

    if (m_color_normal_y > 0x80)
    {
        m_color_outline_y = 0x20;
        m_color_outline_u = m_color_outline_v = 128;
    }
    else
    {
        m_color_outline_y = 0xE0;
        m_color_outline_u = m_color_outline_v = 128;
    }

    m_color_shadow_y = 0x20;
    m_color_shadow_u = m_color_shadow_v = 128;
}

void TTFFont::setColor(QColor color, kTTF_Color k)
{
    float y = (0.299 * color.red()) +
              (0.587 * color.green()) +
              (0.114 * color.blue());
    float u = (0.564 * (color.blue() - y));
    float v = (0.713 * (color.red() - y));

    switch (k)
    {
        case kTTF_Normal:
            m_color_normal_y = (uint8_t)(y);
            m_color_normal_u = (uint8_t)(127 + u);
            m_color_normal_v = (uint8_t)(127 + v);
            break;

        case kTTF_Outline:
            m_color_outline_y = (uint8_t)(y);
            m_color_outline_u = (uint8_t)(127 + u);
            m_color_outline_v = (uint8_t)(127 + v);
            break;

        case kTTF_Shadow:
            m_color_shadow_y = (uint8_t)(y);
            m_color_shadow_u = (uint8_t)(127 + u);
            m_color_shadow_v = (uint8_t)(127 + v);
            break;
    }
}

Raster_Map *TTFFont::create_font_raster(int width, int height)
{
   Raster_Map      *rmap;

   rmap = new Raster_Map;
   rmap->width = (width + 3) & -4;
   rmap->rows = height;
   rmap->cols = rmap->width;
   rmap->size = rmap->rows * rmap->width;
   if (rmap->size <= 0)
   {
        delete rmap;
        return NULL;
   }
   rmap->bitmap = new unsigned char[rmap->size];
   if (!rmap->bitmap)
   {
        delete rmap;
        return NULL;
   }
   memset(rmap->bitmap, 0, rmap->size);
   return rmap;
}

Raster_Map *TTFFont::duplicate_raster(FT_BitmapGlyph bmap)
{
   Raster_Map      *new_rmap;

   new_rmap = new Raster_Map;

   new_rmap->width = bmap->bitmap.width;
   new_rmap->rows = bmap->bitmap.rows;
   new_rmap->cols = bmap->bitmap.pitch;
   new_rmap->size = new_rmap->cols * new_rmap->rows;

   if (new_rmap->size > 0)
   {
	new_rmap->bitmap = new unsigned char[new_rmap->size];
	memcpy(new_rmap->bitmap, bmap->bitmap.buffer, new_rmap->size);
   }
   else
      new_rmap->bitmap = NULL;

   return new_rmap;
}

void TTFFont::clear_raster(Raster_Map * rmap)
{
   if (rmap->bitmap)
       memset(rmap->bitmap, 0, rmap->size);
}

void TTFFont::destroy_font_raster(Raster_Map * rmap)
{
   if (!rmap)
      return;
   if (rmap->bitmap)
      delete [] rmap->bitmap;
   delete rmap;
}

Raster_Map *TTFFont::calc_size(int *width, int *height,
                               const QString &text, bool double_size)
{
   unsigned int i, pw, ph;
   Raster_Map *rtmp;

   pw = 0;
   ph = ((max_ascent) - max_descent) / 64;
   ph = (double_size) ? ph<<1 : ph;

   for (i = 0; i < text.length(); i++)
   {
       unsigned short j = text[i].unicode();

       if (!cache_glyph(j))
           continue;
       if (i == 0)
       {
           FT_Load_Glyph(face, j, FT_LOAD_DEFAULT);
           pw += 2; //((face->glyph->metrics.horiBearingX) / 64);
       }

       if ((i + 1) == text.length())
       {
           FT_BBox bbox;
           FT_Glyph_Get_CBox(glyphs[j], ft_glyph_bbox_subpixels, &bbox);
           pw += (bbox.xMax / 64);
       }
       else
       {
           if (glyphs[j]->advance.x == 0)
               pw += 4;
           else
               pw += glyphs[j]->advance.x / 65535;
       }
   }
   *width = pw + 4;
   *height = ph;

   rtmp = create_font_raster(face->size->metrics.x_ppem + 32, 
                             face->size->metrics.y_ppem + 32);
   return rtmp;
}

void TTFFont::render_text(Raster_Map *rmap, Raster_Map *rchr,
                          const QString &text, int *xorblah, int *yor,
                          bool double_size)
{
   FT_F26Dot6 x, y, xmin, ymin, xmax, ymax;
   FT_BBox bbox;
   unsigned int i, ioff, iread;
   char *off, *off2, *read, *_off, *_off2, *_read;
   int x_offset, y_offset;
   unsigned short j, previous;
   Raster_Map *rtmp;

   j = text[0];
   FT_Load_Glyph(face, j, FT_LOAD_DEFAULT);
   x_offset = 2; //(face->glyph->metrics.horiBearingX) / 64;

   y_offset = -(max_descent / 64);

   *xorblah = x_offset;
   *yor = rmap->rows - y_offset;

   rtmp = NULL;
   previous = 0;

   for (i = 0; i < text.length(); i++)
   {
       j = text[i].unicode();

       if (!FT_VALID(glyphs[j]))
           continue;

       FT_Glyph_Get_CBox(glyphs[j], ft_glyph_bbox_subpixels, &bbox);
       xmin = bbox.xMin & -64;
       ymin = bbox.yMin & -64;
       xmax = (bbox.xMax + 63) & -64;
       ymax = (bbox.yMax + 63) & -64;

       if (glyphs_cached[j])
           rtmp = glyphs_cached[j];
       else
       {
           FT_Vector origin;
           FT_BitmapGlyph bmap;

           rtmp = rchr;
           clear_raster(rtmp);

           origin.x = 0;
           origin.y = 0;

           FT_Glyph_To_Bitmap(&glyphs[j], ft_render_mode_normal, &origin, 1);
           bmap = (FT_BitmapGlyph)(glyphs[j]);

           glyphs_cached[j] = duplicate_raster(bmap);

           rtmp = glyphs_cached[j];
       }

       // Blit-or the resulting small pixmap into the biggest one
       // We do that by hand, and provide also clipping.

#if 0
// Kerning isn't available everywhere...
       if (use_kerning && previous && j)
       {
           FT_Vector delta;
           FT_Get_Kerning(face, previous, j, FT_KERNING_DEFAULT, &delta);
           x_offset += delta.x >> 6;
       }
#endif

       xmin = (xmin >> 6) + x_offset;
       ymin = (ymin >> 6) + y_offset;
       xmax = (xmax >> 6) + x_offset;
       ymax = (ymax >> 6) + y_offset;

       // Take care of comparing xmin and ymin with signed values!
       // This was the cause of strange misplacements when Bit.rows
       // was unsigned.

       if (xmin >= (int)rmap->width ||
           ymin >= (int)rmap->rows ||
           xmax < 0 || ymax < 0)
           continue;

       // Note that the clipping check is performed _after_ rendering
       // the glyph in the small bitmap to let this function return
       // potential error codes for all glyphs, even hidden ones.

       // In exotic glyphs, the bounding box may be larger than the
       // size of the small pixmap.  Take care of that here.

       if (xmax - xmin + 1 > rtmp->width)
           xmax = xmin + rtmp->width - 1;

       if (ymax - ymin + 1 > rtmp->rows)
           ymax = ymin + rtmp->rows - 1;

       // set up clipping and cursors
 
       iread = 0;
       if (ymin < 0)
       {
           iread -= ymin * rtmp->cols;
           ioff = 0;
           ymin = 0;
       }
       else
       {
           int ym = (double_size) ? (ymin << 1) : ymin;
           ioff   = (rmap->rows - ym - 1) * rmap->cols;
       }

       if (ymax >= rmap->rows)
           ymax = rmap->rows - 1;

       if (xmin < 0)
       {
           iread -= xmin;
           xmin = 0;
       }
       else
           ioff += xmin;

       if (xmax >= rmap->width)
           xmax = rmap->width - 1;

       iread = (ymax - ymin) * rtmp->cols + iread;

       _read = (char *)rtmp->bitmap + iread;
       _off = (char *)rmap->bitmap + ioff;
       _off2 = _off - rmap->cols;
       
       for (y = ymin; y <= ymax; y++)
       {
           read = _read;
           off = _off;
           off2 = _off2;

           for (x = xmin; x <= xmax; x++)
           {
               *off = *read;
               if (double_size)
               {
                   *off2 = *read;
                   off2++;
               }
               off++;
               read++;
           }
           _read -= rtmp->cols;
           _off -= rmap->cols;
           if (double_size)
           {
               _off -= rmap->cols;
               _off2 -= rmap->cols;
               _off2 -= rmap->cols;
           }
       }
       if (glyphs[j]->advance.x == 0)
           x_offset += 4;
       else
           x_offset += (glyphs[j]->advance.x / 65535);
       previous = j;
    }
}

void TTFFont::merge_text(OSDSurface *surface, Raster_Map * rmap, int offset_x, 
                         int offset_y, int xstart, int ystart, int width, 
                         int height, int alphamod, kTTF_Color k)
{
    unsigned char * asrc, * ydst, * udst, * vdst, * adst;
    uint8_t color_y = 0, color_u = 0, color_v = 0;

    if (xstart < 0)
    {
        width += xstart;
        offset_x -= xstart;
        xstart = 0;
    }

    if (ystart < 0)
    {
        height += ystart;
        offset_y -= ystart;
        ystart = 0;
    }

    if (height + ystart > surface->height)
        height = surface->height - ystart;

    if (width + xstart > surface->width)
        width = surface->width - xstart;

    QRect drawRect(xstart, ystart, width, height);
    surface->AddRect(drawRect);
    
    asrc = rmap->bitmap + rmap->cols * offset_y + offset_x;
    ydst = surface->y + surface->width * ystart + xstart;
    adst = surface->alpha + surface->width * ystart + xstart;
    udst = surface->u + (surface->width >> 1) * (ystart >> 1) + (xstart >> 1);
    vdst = surface->v + (surface->width >> 1) * (ystart >> 1) + (xstart >> 1);

    switch(k)
    {
        case kTTF_Normal:
            color_y = m_color_normal_y;
            color_u = m_color_normal_u;
            color_v = m_color_normal_v;
            break;
        case kTTF_Outline:
            color_y = m_color_outline_y;
            color_u = m_color_outline_u;
            color_v = m_color_outline_v;
            break;
        case kTTF_Shadow:
            color_y = m_color_shadow_y;
            color_u = m_color_shadow_u;
            color_v = m_color_shadow_v;
            break;
    }

    (surface->blendcolorfunc) (color_y, color_u, color_v,
                               asrc, rmap->width, ydst, udst,
                               vdst, adst, surface->width, width, height,
                               alphamod, 1, surface->rec_lut,
                               surface->pow_lut);
}

void TTFFont::DrawString(OSDSurface *surface, int x, int y, 
                         const QString &text, int maxx, int maxy, 
                         int alphamod, bool double_size)
{
   int                  width, height, w, h, inx, iny, clipx, clipy;
   Raster_Map          *rmap, *rtmp;
   char                 is_pixmap = 0;

   if (text.isEmpty())
        return;

   inx = 0;
   iny = 0;

   rtmp = calc_size(&w, &h, text, double_size);
   if (w <= 0 || h <= 0)
   {
       destroy_font_raster(rtmp);
       return;
   }
   rmap = create_font_raster(w, h);

   render_text(rmap, rtmp, text, &inx, &iny, double_size);

   is_pixmap = 1;

   y += loadedfontsize;
   
   width = maxx;
   height = (double_size) ? maxy<<1 : maxy;

   clipx = 0;
   clipy = 0;

   x = x - inx;
   y = y - iny;

   width = width - x;
   height = height - y;

   if (width > w)
      width = w;
   if (height > h)
      height = h;

   if (x < 0)
   {
       clipx -= x;
       width += x;
       x = 0;
   }

   if (y < 0)
   {
       clipy  -= y;
       height += y;
       y = 0;
   }

   if ((width <= 0) || (height <= 0))
   {
       destroy_font_raster(rmap);
       destroy_font_raster(rtmp);
       return;
   }

   if (m_shadowxoff != 0 || m_shadowyoff != 0)
   {
       merge_text(surface, rmap, clipx, clipy, x + m_shadowxoff,
                  y + m_shadowyoff, width, height, alphamod, kTTF_Shadow);
   }

   if (m_outline)
   {
       merge_text(surface, rmap, clipx, clipy, x - 1, y - 1, width, height,
                  alphamod, kTTF_Outline);
       merge_text(surface, rmap, clipx, clipy, x + 1, y - 1, width, height,
                  alphamod, kTTF_Outline);
       merge_text(surface, rmap, clipx, clipy, x - 1, y + 1, width, height,
                  alphamod, kTTF_Outline);
       merge_text(surface, rmap, clipx, clipy, x + 1, y + 1, width, height,
                  alphamod, kTTF_Outline);
   }

   merge_text(surface, rmap, clipx, clipy, x, y, width, height, alphamod);

   destroy_font_raster(rmap);
   destroy_font_raster(rtmp);
}

TTFFont::~TTFFont()
{
   if (!valid)
       return;

   KillFace();
}

void TTFFont::KillFace(void)
{
    FT_Done_Face(face);
    for (QMap<ushort, Raster_Map*>::Iterator it = glyphs_cached.begin();
         it != glyphs_cached.end(); it++)
    {
        destroy_font_raster(it.data());
    }
    glyphs_cached.clear();

    for (QMap<ushort, FT_Glyph>::Iterator it = glyphs.begin();
         it != glyphs.end(); it++)
    {
        FT_Done_Glyph(it.data());
    }
    glyphs.clear();
}

TTFFont::TTFFont(char *file, int size, float wscale,
                 float hmult)
{
   FT_Error            error;

   valid = false;
   m_size = size;
   spacewidth = 0;

   m_outline = false;
   m_shadowxoff = 0;
   m_shadowyoff = 0;

   m_color_normal_y = 255;
   m_color_normal_u = m_color_normal_v = 128;

   m_color_outline_y = 0x40;
   m_color_outline_u = m_color_outline_v = 128;

   m_color_shadow_y = 0x20;
   m_color_shadow_u = m_color_shadow_v = 128;

   if (!have_library)
   {
	error = FT_Init_FreeType(&the_library);
	if (error) {
	   return;
        }
        have_library++;
   }

   fontsize = size;
   library = the_library;

   m_wscale = wscale;
   m_file = file;
   m_hmult = hmult;

   Init();
}

bool TTFFont::cache_glyph(unsigned short c)
{
    FT_BBox         bbox;
    unsigned short  code;

    if (FT_VALID(glyphs[c]))
        return true;

    code = FT_Get_Char_Index(face, c);

    FT_Load_Glyph(face, code, FT_LOAD_DEFAULT);
    FT_Glyph& glyph = glyphs[c];
    if (FT_Get_Glyph(face->glyph, &glyph))
    {
        cerr << "cannot load glyph:" << hex << c << "\n";
        return false;
    }

    FT_Glyph_Get_CBox(glyph, ft_glyph_bbox_subpixels, &bbox);

    if ((bbox.yMin & -64) < max_descent)
        max_descent = (bbox.yMin & -64);
    if (((bbox.yMax + 63) & -64) > max_ascent)
        max_ascent = ((bbox.yMax + 63) & -64);

    return true;
}

void TTFFont::Init(void)
{
   FT_Error error;
   FT_CharMap char_map;
   int xdpi = 96, ydpi = 96;
   unsigned short i, n;

   error = FT_New_Face(library, m_file, 0, &face);
   if (error)
   {
	return;
   }

   loadedfontsize = (int)(fontsize * m_hmult);
   xdpi = (int)(xdpi * m_wscale);

   FT_Set_Char_Size(face, 0, loadedfontsize * 64, xdpi, ydpi);

   n = face->num_charmaps;

   for (i = 0; i < n; i++)
   {
        char_map = face->charmaps[i];
	if ((char_map->platform_id == 3 && char_map->encoding_id == 1) ||
	    (char_map->platform_id == 0 && char_map->encoding_id == 0))
	{
	     FT_Set_Charmap(face, char_map);
	     break;
	}
   }
   if (i == n)
      FT_Set_Charmap(face, face->charmaps[0]);

   max_descent = 0;
   max_ascent = 0;

   for (i = 0; i < 256; ++i)
        cache_glyph(i);

   use_kerning = FT_HAS_KERNING(face);

   valid = true;

   int mwidth, twidth;
   CalcWidth("M M", &twidth);
   CalcWidth("M", &mwidth);

   spacewidth = twidth - (mwidth * 2);
}

void TTFFont::Reinit(float wscale, float hmult)
{
    m_wscale = wscale;
    m_hmult = hmult;

    KillFace();
    Init();
}


void TTFFont::CalcWidth(const QString &text, int *width_return)
{
   unsigned int i, pw;

   pw = 0;

   for (i = 0; i < text.length(); i++)
   {
	unsigned short j = text[i].unicode();

	if (!cache_glyph(j))
	   continue;

        if (glyphs[j]->advance.x == 0)
            pw += 4;
        else
	    pw += glyphs[j]->advance.x / 65535;
   }

   if (width_return)
      *width_return = pw;
}
