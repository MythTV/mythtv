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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ttfont.h"


static char         have_library = 0;
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

Raster_Map *TTFFont::calc_size(int *width, int *height, char *text)
{
   int                 i, pw, ph;
   Raster_Map         *rtmp;

   pw = 0;
   ph = ((max_ascent) - max_descent) / 64;

   for (i = 0; text[i]; i++)
   {
       unsigned char       j = text[i];

       if (!FT_VALID(glyphs[j]))
           continue;
       if (i == 0)
       {
           FT_Load_Glyph(face, j, FT_LOAD_DEFAULT);
           pw += 2; //((face->glyph->metrics.horiBearingX) / 64);
       }
       if (text[i + 1] == 0)
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
   *width = pw;
   *height = ph;

   rtmp = create_font_raster(face->size->metrics.x_ppem + 32, 
                             face->size->metrics.y_ppem + 32);
   return rtmp;
}

void TTFFont::render_text(Raster_Map *rmap, Raster_Map *rchr, char *text,
	                  int *xorblah, int *yor)
{
   FT_F26Dot6          x, y, xmin, ymin, xmax, ymax;
   FT_BBox             bbox;
   int                 i, ioff, iread;
   char               *off, *read, *_off, *_read;
   int                 x_offset, y_offset;
   unsigned char       j, previous;
   Raster_Map         *rtmp;

   j = text[0];
   FT_Load_Glyph(face, j, FT_LOAD_DEFAULT);
   x_offset = 2; //(face->glyph->metrics.horiBearingX) / 64;

   y_offset = -(max_descent / 64);

   *xorblah = x_offset;
   *yor = rmap->rows - y_offset;

   rtmp = NULL;
   previous = 0;

   for (i = 0; text[i]; i++)
   {
       j = text[i];

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

           origin.x = -xmin;
           origin.y = -ymin;

           FT_Glyph_To_Bitmap(&glyphs[j], ft_render_mode_normal, &origin, 1);
           bmap = (FT_BitmapGlyph)(glyphs[j]);

           glyphs_cached[j] = duplicate_raster(bmap);

           rtmp = glyphs_cached[j];
       }
       // Blit-or the resulting small pixmap into the biggest one
       // We do that by hand, and provide also clipping.

       //if (use_kerning && previous && j)
       //{
       //    FT_Vector delta;
       //    FT_Get_Kerning(face, previous, j, FT_KERNING_DEFAULT, &delta);
       //    x_offset += delta.x >> 6;
       //}

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
           ioff = (rmap->rows - ymin - 1) * rmap->cols;

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

       for (y = ymin; y <= ymax; y++)
       {
           read = _read;
           off = _off;

           for (x = xmin; x <= xmax; x++)
           {
	       *off = *read;
               off++;
               read++;
           }
           _read -= rtmp->cols;
           _off -= rmap->cols;
       }
       if (glyphs[j]->advance.x == 0)
           x_offset += 4;
       else
           x_offset += (glyphs[j]->advance.x / 65535);
       previous = j;
    }
}

void TTFFont::merge_text(unsigned char *yuv, Raster_Map * rmap, int offset_x, 
                         int offset_y, int xstart, int ystart, int width, 
                         int height, int video_width, int video_height, 
                         bool white, int alphamod)
{
   int                 x, y;
   unsigned char      *ptr, *src;
   unsigned char       a;

   unsigned char *uptr, *usrc;
   unsigned char *vptr, *vsrc;

   uptr = yuv + video_height * video_width;
   vptr = uptr + (video_height * video_width) / 4;   
   int offset; 

   if (height + ystart > video_height)
       height = video_height - ystart - 1;

   int tmp1, tmp2;
 
   for (y = 0; y < height; y++)
     {
	ptr = (unsigned char *)rmap->bitmap + offset_x + 
              ((y + offset_y) * rmap->cols);
	for (x = 0; x < width; x++)
	  {
	     if ((a = *ptr) > 0)
	       {
                  a = ((a * alphamod) + 0x80) >> 8;
		  src = yuv + (y + ystart) * video_width + (x + xstart);
		  if (white)
                  {
                      tmp1 = (255 - *src) * a;
                      tmp2 = *src + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                      *src = tmp2 & 0xff;
                      if (a >= 230)
                      {
                          offset = ((y + ystart) / 2) * (video_width / 2) + 
                                   (x + xstart) / 2;
                          usrc = uptr + offset; 
                          vsrc = vptr + offset; 
                          *usrc = 128;
                          *vsrc = 128;
                      }
                  }
		  else
                  {
                      tmp1 = (0 - *src) * a;
                      tmp2 = *src + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                      *src = tmp2 & 0xff;
                  }
	       }
	     ptr++;
	  }
     }
}

void TTFFont::DrawString(unsigned char *yuvptr, int x, int y, 
                         const QString &text, int maxx, int maxy, 
                         int alphamod, bool white, bool rightjustify)
{
   int                  width, height, w, h, inx, iny, clipx, clipy;
   Raster_Map          *rmap, *rtmp;
   char                 is_pixmap = 0;

   if (text.length() < 1)
        return;

   int video_width = vid_width;
   int video_height = vid_height;

   char *ctext = (char *)text.latin1(); 

   inx = 0;
   iny = 0;

   rtmp = calc_size(&w, &h, (char *)ctext);
   if (w <= 0 || h <= 0)
   {
       destroy_font_raster(rtmp);
       return;
   }
   rmap = create_font_raster(w, h);

   render_text(rmap, rtmp, ctext, &inx, &iny);

   is_pixmap = 1;

   y += fontsize;
   
   width = maxx;
   height = maxy;

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
	clipx = -x;
	width += x;
	x = 0;
     }
   if (y < 0)
     {
	clipy = -y;
	height += y;
	y = 0;
     }
   if ((width <= 0) || (height <= 0))
     {
	destroy_font_raster(rmap);
	destroy_font_raster(rtmp);
	return;
     }

   if (rightjustify)
   {
   }

   merge_text(yuvptr, rmap, clipx, clipy, x, y, width, height, 
              video_width, video_height, white, alphamod);

   destroy_font_raster(rmap);
   destroy_font_raster(rtmp);
}

TTFFont::~TTFFont()
{
   int                 i;

   if (!valid)
       return;

   FT_Done_Face(face);
   for (i = 0; i < num_glyph; i++)
     {
	if (glyphs_cached[i])
	   destroy_font_raster(glyphs_cached[i]);
	if (FT_VALID(glyphs[i]))
	   FT_Done_Glyph(glyphs[i]);
     }
   if (glyphs)
      free(glyphs);
   if (glyphs_cached)
      free(glyphs_cached);

   have_library--;

   if (!have_library)
       FT_Done_FreeType(the_library);
}

TTFFont::TTFFont(char *file, int size, int video_width, int video_height)
{
   FT_Error            error;
   FT_CharMap          char_map;
   FT_BBox             bbox;
   int                 xdpi = 96, ydpi = 96;
   unsigned short      i, n, code;

   valid = false;
   m_size = size;
   spacewidth = 0;

   if (!have_library)
   {
	error = FT_Init_FreeType(&the_library);
	if (error) {
	   return;
        }
   }
   have_library++;

   fontsize = size;
   library = the_library;
   error = FT_New_Face(library, file, 0, &face);
   if (error)
   {
        have_library--;
 
        if (!have_library)
            FT_Done_FreeType(the_library);

	return;
   }

   if (video_width != video_height * 4 / 3)
   {
       xdpi = (int)(xdpi * 
              (float)(video_width / (float)(video_height * 4 / 3)));
   }

   vid_width = video_width;
   vid_height = video_height;

   FT_Set_Char_Size(face, 0, size * 64, xdpi, ydpi);

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

   num_glyph = face->num_glyphs;
   num_glyph = 256;
   glyphs = (FT_Glyph *) malloc((num_glyph + 1) * sizeof(FT_Glyph));
   memset(glyphs, 0, num_glyph * sizeof(FT_Glyph));
   glyphs_cached = (Raster_Map **) malloc(num_glyph * sizeof(Raster_Map *));
   memset(glyphs_cached, 0, num_glyph * sizeof(Raster_Map *));

   max_descent = 0;
   max_ascent = 0;

   for (i = 0; i < num_glyph; ++i)
   {
	if (FT_VALID(glyphs[i]))
	   continue;

	code = FT_Get_Char_Index(face, i);

	FT_Load_Glyph(face, code, FT_LOAD_DEFAULT);
        FT_Get_Glyph(face->glyph, &glyphs[i]);

        FT_Glyph_Get_CBox(glyphs[i], ft_glyph_bbox_subpixels, &bbox);

	if ((bbox.yMin & -64) < max_descent)
	   max_descent = (bbox.yMin & -64);
	if (((bbox.yMax + 63) & -64) > max_ascent)
	   max_ascent = ((bbox.yMax + 63) & -64);
   }

   use_kerning = 0; //FT_HAS_KERNING(face);

   valid = true;

   int mwidth, twidth;
   CalcWidth("M M", &twidth);
   CalcWidth("M", &mwidth);

   spacewidth = twidth - (mwidth * 2);
}

void TTFFont::CalcWidth(const QString &text, int *width_return)
{
   int                 i, pw;

   char *ctext = (char *)text.ascii();
   
   pw = 0;

   for (i = 0; ctext[i]; i++)
   {
	unsigned char       j = ctext[i];

	if (!FT_VALID(glyphs[j]))
	   continue;
        if (glyphs[j]->advance.x == 0)
            pw += 4;
        else
	    pw += glyphs[j]->advance.x / 65535;
   }

   if (width_return)
      *width_return = pw;
}
