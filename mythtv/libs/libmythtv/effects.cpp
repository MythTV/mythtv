// a linear blending deinterlacer yoinked from the mplayer sources.

#include <stdlib.h>

#ifdef MMX
#include "mmx.h"
#define PAVGB(a,b)  "pavgb " #a ", " #b " \n\t"
#else
#define emms()
#endif

static inline void linearBlend(unsigned char *src, int stride)
{
#ifdef MMX
//  src += 4 * stride;
  asm volatile(
       "leal (%0, %1), %%eax                           \n\t"
       "leal (%%eax, %1, 4), %%ebx                     \n\t"

       "movq (%0), %%mm0                               \n\t" // L0
       "movq (%%eax, %1), %%mm1                        \n\t" // L2
       PAVGB(%%mm1, %%mm0)                                   // L0+L2
       "movq (%%eax), %%mm2                            \n\t" // L1
       PAVGB(%%mm2, %%mm0)
       "movq %%mm0, (%0)                               \n\t"
       "movq (%%eax, %1, 2), %%mm0                     \n\t" // L3
       PAVGB(%%mm0, %%mm2)                                   // L1+L3
       PAVGB(%%mm1, %%mm2)                                   // 2L2 + L1 + L3
       "movq %%mm2, (%%eax)                            \n\t"
       "movq (%0, %1, 4), %%mm2                        \n\t" // L4
       PAVGB(%%mm2, %%mm1)                                   // L2+L4
       PAVGB(%%mm0, %%mm1)                                   // 2L3 + L2 + L4
       "movq %%mm1, (%%eax, %1)                        \n\t"
       "movq (%%ebx), %%mm1                            \n\t" // L5
       PAVGB(%%mm1, %%mm0)                                   // L3+L5
       PAVGB(%%mm2, %%mm0)                                   // 2L4 + L3 + L5
       "movq %%mm0, (%%eax, %1, 2)                     \n\t"
       "movq (%%ebx, %1), %%mm0                        \n\t" // L6
       PAVGB(%%mm0, %%mm2)                                   // L4+L6
       PAVGB(%%mm1, %%mm2)                                   // 2L5 + L4 + L6
       "movq %%mm2, (%0, %1, 4)                        \n\t"
       "movq (%%ebx, %1, 2), %%mm2                     \n\t" // L7
       PAVGB(%%mm2, %%mm1)                                   // L5+L7
       PAVGB(%%mm0, %%mm1)                                   // 2L6 + L5 + L7
       "movq %%mm1, (%%ebx)                            \n\t"
       "movq (%0, %1, 8), %%mm1                        \n\t" // L8
       PAVGB(%%mm1, %%mm0)                                   // L6+L8
       PAVGB(%%mm2, %%mm0)                                   // 2L7 + L6 + L8
       "movq %%mm0, (%%ebx, %1)                        \n\t"
       "movq (%%ebx, %1, 4), %%mm0                     \n\t" // L9
       PAVGB(%%mm0, %%mm2)                                   // L7+L9
       PAVGB(%%mm1, %%mm2)                                   // 2L8 + L7 + L9
       "movq %%mm2, (%%ebx, %1, 2)                     \n\t"

       : : "r" (src), "r" (stride)
       : "%eax", "%ebx"
  );
#else
  int x;
//  src += 4 * stride;
  for (x=0; x<8; x++)
  {
     src[0       ] = (src[0       ] + 2*src[stride  ] + src[stride*2])>>2;
     src[stride  ] = (src[stride  ] + 2*src[stride*2] + src[stride*3])>>2;
     src[stride*2] = (src[stride*2] + 2*src[stride*3] + src[stride*4])>>2;
     src[stride*3] = (src[stride*3] + 2*src[stride*4] + src[stride*5])>>2;
     src[stride*4] = (src[stride*4] + 2*src[stride*5] + src[stride*6])>>2;
     src[stride*5] = (src[stride*5] + 2*src[stride*6] + src[stride*7])>>2;
     src[stride*6] = (src[stride*6] + 2*src[stride*7] + src[stride*8])>>2;
     src[stride*7] = (src[stride*7] + 2*src[stride*8] + src[stride*9])>>2;

     src++;
  }
#endif
}


void linearBlendYUV420(unsigned char *yuvptr, int width, int height)
{
  int stride = width;
  int ymax = height - 8;
  int x,y;
  unsigned char *src;

  for (y = 0; y < ymax; y+=8)
  {
    for (x = 0; x < stride; x+=8)
    {
       src = yuvptr + x + y * stride;  
       linearBlend(src, stride);  
    }
  }

  stride = width / 2;
  ymax = height / 2 - 8;
  
  unsigned char *uoff = yuvptr + width * height;
  unsigned char *voff = yuvptr + width * height * 5 / 4;
 
  for (y = 0; y < ymax; y += 8)
  {
    for (x = 0; x < stride; x += 8)
    {
       src = uoff + x + y * stride;
       linearBlend(src, stride);

       src = voff + x + y * stride;
       linearBlend(src, stride);
     }
  }

  emms();
}


static int vid_width;
static int vid_height;
static int info_y_start;
static int info_y_end;
static int info_x_start;
static int info_x_end;
static int info_width;
static int info_height;

static int displayframes;
static int darken_info;

void InitializeOSD(int width, int height)
{
    vid_width = width;
    vid_height = height;

    info_y_start = height * 5 / 8;
    info_y_end = height * 15 / 16;
    info_x_start = width / 8;
    info_x_end = width * 7 / 8;

    info_width = info_x_end - info_x_start;
    info_height = info_y_end - info_y_start;

    displayframes = 0;
}

void SetOSDInfoText(char *text, int length)
{
    displayframes = length;
    darken_info = true;
}

void DisplayOSD(unsigned char *yuvptr)
{
    if (displayframes <= 0)
    {
        darken_info = false; 
    	return;
    }

    displayframes--;

    unsigned char *src;

    char c = 128;
    if (darken_info)
    {
        for (int y = info_y_start ; y < info_y_end; y++)
        {
            for (int x = info_x_start; x < info_x_end; x++)
            {
                src = yuvptr + x + y * vid_width;
	        *src = ((*src * c) >> 8) + *src;
            }
        }
    }
}
