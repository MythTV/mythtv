/*
 * crop v 0.2
 * (C)opyright 2003, Debabrata Banerjee
 * GNU GPL 2 or later
 * 
 * Pass options as crop=top:left:bottom:right as number of 16 pixel blocks.
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "filter.h"
#include "frame.h"
#include "dsputil.h"

#ifdef MMX
#include "i386/mmx.h"
#endif // !MMX

static const char FILTER_NAME[] = "crop";

typedef struct ThisFilter
{
  VideoFilter vf;

  int Chroma_plane_size;
  int Luma_plane_size;
 
  int width;
  int height;

  int cropY1, cropY2;
  int cropC1, cropC2, cropC3;
  int xcrop1, xcrop2;
  int xcropYint, xcropCint, xcropend;
  TF_STRUCT;

} ThisFilter;

int crop(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;
  uint64_t *buf=(uint64_t *)frame->buf;
  int x,y;
  const uint64_t Y_black=0x1010101010101010LL;
  const uint64_t UV_black=0x8080808080808080LL;
  TF_VARS;
  
  TF_START;
  for(y = 0; y < tf->cropY1; y += 2) {// Y Luma
    buf[y] = Y_black;
    buf[y + 1] = Y_black;
  }

  for(y = tf->cropY2; y < tf->Luma_plane_size; y += 2) {
    buf[y] = Y_black;
    buf[y + 1] = Y_black;
  }
  
  for(y = tf->Luma_plane_size; y < tf->cropC1; y++) {// Y Chroma
    buf[y] = UV_black;
    buf[y + tf->Chroma_plane_size] = UV_black;
  }

  for(y = tf->cropC2; y < tf->cropC3; y++) {
    buf[y] = UV_black;
    buf[y + tf->Chroma_plane_size] = UV_black;
  }
 
  for(y = tf->cropY1; y < tf->cropY2; y += tf->xcropYint) { // X Luma
    for(x = 0; x <  tf->xcrop1; x++) {
      buf[y + (x << 1)] = Y_black;
      buf[y + (x << 1) + 1] = Y_black;
    }
    for(x = tf->xcrop2; x < tf->xcropend; x++) {
      buf[y + (x << 1)] = Y_black;
      buf[y + (x << 1) + 1] = Y_black;
    }
  }

  for(y = tf->cropC1; y < tf->cropC2; y+= tf->xcropCint) { // X Chroma
    for(x = 0; x <  tf->xcrop1; x++) {
      buf[y + x] = UV_black;
      buf[y + x + tf->Chroma_plane_size] = UV_black;
    }
    for(x = tf->xcrop2; x < tf->xcropend; x++) {
      buf[y + x] = UV_black;
      buf[y + x + tf->Chroma_plane_size] = UV_black;
    }
  }
 
  TF_END(tf, "Crop: ");
  return 0;
}

#ifdef MMX
int cropMMX(VideoFilter *f, VideoFrame *frame)
{
  ThisFilter *tf = (ThisFilter *)f;  
  uint64_t *buf=(uint64_t *)frame->buf;
  int y,x; 
  const uint64_t Y_black=0x1010101010101010LL;
  const uint64_t UV_black=0x8080808080808080LL;
  TF_VARS;

  TF_START;

  asm volatile("movq (%1),%%mm0    \n\t"	       
	       "movq (%0),%%mm1    \n\t"
	       : : "r" (&UV_black), "r"(&Y_black));
  
  for(y = 0; y < tf->cropY1; y += 2) { // Y Luma
    asm volatile("movq %%mm0, (%0)    \n\t"
		 "movq %%mm0, 8(%0)    \n\t"
		 : : "r" (buf + y));
  }

  for(y = tf->cropY2; y < tf->Luma_plane_size; y += 2) {
    asm volatile("movq %%mm0, (%0)    \n\t"
		 "movq %%mm0, 8(%0)    \n\t"
		 : : "r" (buf + y));
  }

  for(y = tf->Luma_plane_size; y < tf->cropC1; y++) // Y Chroma
    asm volatile("movq %%mm1, (%0)    \n\t"
		 "movq %%mm1, (%1)    \n\t"
		 : : "r" (buf + y), "r" (buf + y + tf->Chroma_plane_size));

  for(y = tf->cropC2; y < tf->cropC3; y++)
    asm volatile("movq %%mm1, (%0)    \n\t"
		 "movq %%mm1, (%1)    \n\t"
		 : : "r" (buf + y), "r" (buf + y + tf->Chroma_plane_size));
 
  for(y = tf->cropY1; y < tf->cropY2; y += tf->xcropYint) { // X Luma
    for(x = 0; x < tf->xcrop1; x++)
      asm volatile("movq %%mm0, (%0)    \n\t"
		   "movq %%mm0, 8(%0)    \n\t"
		   : : "r" (buf + y + (x << 1)));
    for(x = tf->xcrop2; x < tf->xcropend; x++)
      asm volatile("movq %%mm0, (%0)    \n\t"
		   "movq %%mm0, 8(%0)    \n\t"
		   : : "r" (buf + y + (x << 1)));
  }

  for(y = tf->cropC1; y < tf->cropC2; y += tf->xcropCint) { // X Chroma
    for(x=0; x < tf->xcrop1; x++)
      asm volatile("movq %%mm1, (%0)    \n\t"
		   "movq %%mm1, (%1)    \n\t"
		   : : "r" (buf + y + x), "r" (buf + y + x + tf->Chroma_plane_size));
    for(x = tf->xcrop2; x < tf->xcropend; x++)
      asm volatile("movq %%mm1, (%0)    \n\t"
		   "movq %%mm1, (%1)    \n\t"
		   : : "r" (buf + y + x), "r" (buf + y + x + tf->Chroma_plane_size));
  }

  asm volatile("emms\n\t");

  TF_END(tf, "Crop: ");
  return 0;
}
#else // MMX
int cropMMX(VideoFilter *f, VideoFrame *frame)
{ 
  fprintf(stderr,"cropMMX: attempt to use MMX version of crop on non-mmx system\n");
  return crop(f, frame);
}
#endif

VideoFilter *new_filter(VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                        int *width, int *height, char *options)
{
  unsigned int Param1, Param2, Param3, Param4;
  ThisFilter *filter;

  int yp1,yp2,xp1,xp2;

  if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
    {
      fprintf(stderr,"crop: attempt to initialize with unsupported format\n");
      return NULL;
    }

  filter = malloc(sizeof(ThisFilter));

  if (filter == NULL)
    {
      fprintf(stderr,"Couldn't allocate memory for filter\n");
      return NULL;
    }

  filter->Luma_plane_size = (*width * *height) / 8;
  filter->Chroma_plane_size = (*height * *width / 4) / 8;
  filter->cropC3 = filter->Luma_plane_size + filter->Chroma_plane_size;
  filter->xcropend = *width / 16;
  filter->xcropYint = *width / 8;
  filter->xcropCint = *width / 16;

  if (options && (sscanf(options, "%u:%u:%u:%u", &Param1, &Param2, &Param3, &Param4) == 4)) { 
    yp1 = (uint8_t) Param1;
    yp2 = (uint8_t) Param3;
    xp1 = (uint8_t) Param2;
    xp2 = (uint8_t) Param4;
  }
  else {
    yp1 = 1;
    yp2 = 1;
    xp1 = 1;
    xp2 = 1;
  }

  // linear addresses of 8-byte-block planes
  filter->cropY1 = yp1 * *width * 2;
  filter->cropY2 = ((*height/16) - yp2) * *width * 2;
  filter->cropC1 = filter->Luma_plane_size+(*width * (yp1 * 4) / 8);
  filter->cropC2 = filter->Luma_plane_size + ((*width * (*height/16 - yp2) * 4) / 8);
  filter->xcrop1 = xp1;
  filter->xcrop2 = (*width/16) - xp2;
  
  if (mm_support()&MM_MMX) filter->vf.filter = &cropMMX;
  else filter->vf.filter = &crop;

  filter->vf.cleanup = NULL;
  TF_INIT(filter);

  return (VideoFilter *)filter;
}

static FmtConv FmtList[] = 
  {
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
  };

FilterInfo filter_table[] = 
  {
    {
      symbol:     "new_filter",
      name:       "crop",
      descript:   "crops picture by macroblock intervals",
      formats:    FmtList,
      libname:    NULL
    },
    FILT_NULL
  };
