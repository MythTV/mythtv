// a linear blending deinterlacer yoinked from the mplayer sources.

#include <stdlib.h>
#include <stdio.h>

// By default, these routines use MMX instructions.  If your CPU does not
// support these, uncomment the following line
//#undef MMX

#ifdef MMX
#include "mmx.h"
#define PAVGB(a,b)  "pavgb " #a ", " #b " \n\t"
#else
#define emms()
#endif

#include "filter.h"
#include "frame.h"

static const char FILTER_NAME[] = "linearblend";

typedef struct ThisFilter
{
  int (*filter)(VideoFilter *, Frame *);
  void (*cleanup)(VideoFilter *);

  char *name;
  void *handle; // Library handle;

  /* functions and variables below here considered "private" */

} ThisFilter;

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

int linearBlendFilter(VideoFilter *vf, Frame *frame)
{
  int width = frame->width;
  int height = frame->height;
  unsigned char *yuvptr = frame->buf;
  int stride = width;
  int ymax = height - 8;
  int x,y;
  unsigned char *src;
  unsigned char *uoff;
  unsigned char *voff;
  vf = vf;

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
  
  uoff = yuvptr + width * height;
  voff = yuvptr + width * height * 5 / 4;
 
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

  return 0;
}

void cleanup(VideoFilter *filter)
{
  free(filter);
}

VideoFilter *new_filter(char *options)
{
  ThisFilter *filter = malloc(sizeof(ThisFilter));

  options = options;

  if (filter == NULL)
  {
    fprintf(stderr,"Couldn't allocate memory for filter\n");
    return NULL;
  }

  filter->filter=&linearBlendFilter;
  filter->cleanup=&cleanup;
  filter->name = (char *)FILTER_NAME;
  return (VideoFilter *)filter;
}

